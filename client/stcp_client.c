#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include "stcp_client.h"
#include "common.h"
#include "seg.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

//tcb连接池
static client_tcb_t *tcbs[MAX_TRANSPORT_CONNECTIONS];

//记录模拟网络层所使用的连接套接字
static int son_connection;

//记录seghandler线程的tid
pthread_t handler_tid;

void stcp_client_init(int conn)
{
    for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        tcbs[i] = NULL;
    }
    log("client TCB pool has been initialized.");

    //启动接收网络层报文段的线程
    son_connection = conn;
    pthread_create(&handler_tid, NULL, seghandler, NULL);
    log("seghandler started.");
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port.
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接.
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port)
{
    for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        if(tcbs[i] == NULL) {
            client_tcb_t *tcb = calloc(1, sizeof(*tcb));

            tcb->client_portNum = client_port;
            tcb->server_portNum = -1;
            tcb->state = CLOSED;

            //init mutex
            tcb->bufMutex = malloc(sizeof(*tcb->bufMutex));
            pthread_mutex_init(tcb->bufMutex, NULL);

            log("Assign socket %d to port %d", i, client_port);
            tcbs[i] = tcb;

            //socket num
            return i;
        }
    }

    //没有找到条目
    return -1;
}

//发送报文
static inline void
send_ctrl(unsigned short type, unsigned short src_port, unsigned short dst_port) {
    seg_t syn = {
        .header.src_port = src_port,
        .header.dest_port = dst_port,
        .header.length = 0,
        .header.type = type,
    };
    if(sip_sendseg(son_connection, &syn) == -1) {
        log("sending ctrl to port %d failed", dst_port);
    }
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传.
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

static void *timer(void *arg)
{
    // thanks to http://stackoverflow.com/a/9799466/5164297
    client_tcb_t *tcb = arg;
    if (select(0, NULL, NULL, NULL, &tcb->timeout) < 0) {
        perror("select");
    }
    tcb->is_time_out = 1;
    return arg;
}

int stcp_client_connect(int sockfd, unsigned int server_port)
{
    client_tcb_t *tcb = tcbs[sockfd];

    if(tcb == NULL) {
        log("Invalid stcp socket %d",sockfd);
        return 0;
    }
    else if(tcb->state != CLOSED) {
        log("The state of this stcp socket is not CLOSED");
        return 0;
    }
    else {
        tcb->server_portNum = server_port;
        tcb->state = SYNSENT;
        log("Shift state to SYNSENT");

        //for(int i = 0; i < SYN_MAX_RETRY; i++) {
        for (;;) {
            send_ctrl(SYN, tcb->client_portNum, server_port);

            tcb->timeout.tv_sec = SYN_TIMEOUT / 1000000000;
            tcb->timeout.tv_usec = (SYN_TIMEOUT % 1000000000) / 1000000;
            tcb->is_time_out = 0;

            pthread_t tid;
            pthread_create(&tid, NULL, timer, tcb);

            while (tcb->state != CONNECTED && !tcb->is_time_out) {}
            if (tcb->state == CONNECTED) {
                log("connection %d shifts into CONNECTED", sockfd);
                return 1;
            }
            log("oops, syn retry %d.", tcb->state);
        }
        log("oops, syn failed.");
        return -1;
    }
    return 0;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length)
{
    return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd)
{
    client_tcb_t *tcb = tcbs[sockfd];

    if(tcb == NULL) {
        log("Invalid stcp socket %d", sockfd);
        return 0;
    }
    else if(tcb->state != CONNECTED) {
        log("The state of this stcp socket is not CONNECTED");
        return 0;
    }
    else {
        log("Shift state to FINWAIT");
        send_ctrl(FIN, tcb->client_portNum, tcb->server_portNum);
        tcb->state = FINWAIT;

        //设置等待时间
        //for (int i = 0; i < FIN_MAX_RETRY; i++) {
        for (;;) {
            tcb->timeout.tv_sec = FIN_TIMEOUT / 1000000000;
            tcb->timeout.tv_usec = (FIN_TIMEOUT % 1000000000) / 1000000;
            tcb->is_time_out = 0;

            pthread_t tid;
            pthread_create(&tid, NULL, timer, tcb);

            while (tcb->state != CLOSED && !tcb->is_time_out) {}
            if (tcb->state == CLOSED) {
                log("connection %d shifts into CLOSED", sockfd);
                return 0;
            }

            log("oops, fin miss, retry");
        }

        log("oops, fin failed");
        return -1;
    }
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd)
{
    client_tcb_t *tcb = tcbs[sockfd];
    tcbs[sockfd] = NULL;

    //pthread_mutex_destory?
    free(tcb->bufMutex);
    if(tcb->sendBufHead) free(tcb->sendBufHead);
    if(tcb->sendBufunSent) free(tcb->sendBufunSent);
    if(tcb->sendBufTail) free(tcb->sendBufTail);
    free(tcb);

    return 0;
}

//客户端状态机
static void client_fsm(client_tcb_t *tcb, seg_t *seg) {
    switch(tcb->state) {
    case CLOSED:
        log("Unexpected CLOSE state");
        break;
    case SYNSENT:
        switch(seg->header.type) {
        case SYNACK:
            tcb->state = CONNECTED;
            log("%p enters CONNECTED state", tcb);
            break;
        default:
            log("Unexpect segment type %02x for SYNSENT state",seg->header.type);
        }
        break;
    case CONNECTED:
        break;
    case FINWAIT:
        switch(seg->header.type) {
        case FINACK:
            tcb->state = CLOSED;
            log("return closed state");
            break;
        default:
            log("Unexpect segment type %02x for FINWAIT state",seg->header.type);
        }
        break;
    default:
        log("Unexpect tcb state");
    }
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段.
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.

void *seghandler(void* arg) {
    for(;;) {
        seg_t seg = {};
        int result = sip_recvseg(son_connection, &seg);
        if(result == -1) {
            //断开连接
            break;
        }
        else if(result == 1) {
            //丢包
            continue;
        }

        for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
            if(tcbs[i] && tcbs[i]->client_portNum == seg.header.dest_port) {
                client_fsm(tcbs[i], &seg);
            }
        }
    }

    return arg;
}
