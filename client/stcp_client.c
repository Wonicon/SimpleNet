#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "stcp_client.h"
#include "common.h"

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

/**
 * @brief 内部日志信息，自动输出 tcb 相关内容。
 */
#define LOG(tcb, fmt, ...) \
    log(MAGENTA "{tcb:%p} " NORMAL fmt, tcb, ## __VA_ARGS__)

/**
 * @brief The string table for client states
 */
static const char *client_state_s[] = {
#define S(x) #x
#define TOKEN(x) ORANGE S(x) NORMAL
#include "stcp_client_state.h"
#undef TOKEN
#undef S
};

/**
 * @brief Get the state string of the given tcb
 * @param tcb the tcb we want to log its state
 *
 * The tcb can be released in another thread, so
 * we might access a dangling pointer.
 */
static const char *state_to_s(const client_tcb_t *tcb)
{
    return client_state_s[tcb->state];
}

//tcb连接池
static client_tcb_t *tcbs[MAX_TRANSPORT_CONNECTIONS];

//记录模拟网络层所使用的连接套接字
static int son_connection;

//记录seghandler线程的tid
pthread_t handler_tid;

void stcp_client_init(int conn)
{
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
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
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        if (tcbs[i] == NULL) {
            client_tcb_t *tcb = calloc(1, sizeof(*tcb));

            tcb->client_portNum = client_port;
            tcb->server_portNum = (unsigned int)(-1);
            tcb->state = CLOSED;

            //init mutex
            tcb->bufMutex = malloc(sizeof(*tcb->bufMutex));
            pthread_mutex_init(tcb->bufMutex, NULL);
            tcb->bufCond = malloc(sizeof(*tcb->bufCond));
            pthread_cond_init(tcb->bufCond, NULL);

            // init fields related to send
            tcb->sendBufHead = NULL;
            tcb->sendBufTail = NULL;
            tcb->sendBufunSent = NULL;
            tcb->next_seqNum = 0;
            tcb->unAck_segNum = 0;
            tcb->send_time = 0;

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
static inline int
send_ctrl(unsigned short type, unsigned short src_port, unsigned short dst_port)
{
    seg_t syn = {
        .header.src_port = src_port,
        .header.dest_port = dst_port,
        .header.length = 0,
        .header.type = type,
    };
    if (sip_sendseg(son_connection, &syn) == -1) {
        log("sending ctrl to port %d failed", dst_port);
        return -1;
    } else {
        return 1;
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
    const unsigned short prev = tcb->state;
    // TODO We need a lock!
    if (prev != SYNSENT && prev != FINWAIT) {
        LOG(tcb, "does not need to wait");
        return arg;
    }
    LOG(tcb, "timer starts under %s", state_to_s(tcb));
    if (select(0, NULL, NULL, NULL, &tcb->timeout) < 0) {
        perror("select");
    }
    if (prev == tcb->state) {
        LOG(tcb, "timer ends under the same state %s", state_to_s(tcb));
        tcb->is_time_out = 1;  // Allow logs to keep sequence
    } else {
        LOG(tcb, "timer ends from %s to %s", client_state_s[prev], state_to_s(tcb));
    }
    return arg;
}

/**
 * @brief connect to a remote server
 */
int stcp_client_connect(int sockfd, unsigned int server_port)
{
    client_tcb_t *tcb = tcbs[sockfd];

    if (tcb == NULL) {
        log("Invalid stcp socket %d", sockfd);
        return 0;
    } else if (tcb->state != CLOSED) {
        log("The state of this stcp socket is not CLOSED");
        return 0;
    } else {
        tcb->server_portNum = server_port;
        tcb->state = SYNSENT;
        LOG(tcb, "shifts into %s", state_to_s(tcb));

        for (int i = 0; i < SYN_MAX_RETRY; i++) {
            if (send_ctrl(SYN, tcb->client_portNum, server_port) == -1) {
                // 连接断开，直接退出。
                tcb->state = CLOSED;
                break;
            }

            tcb->timeout.tv_sec = SYN_TIMEOUT / 1000000000;
            tcb->timeout.tv_usec = (SYN_TIMEOUT % 1000000000) / 1000000;
            tcb->is_time_out = 0;

            pthread_t tid;
            pthread_create(&tid, NULL, timer, tcb);

            while (tcb->state != CONNECTED && !tcb->is_time_out) {}
            if (tcb->state == CONNECTED) {
                LOG(tcb, "connection %d shifts into %s", sockfd, state_to_s(tcb));
                return 1;
            } else {
                LOG(tcb, "%s time out", state_to_s(tcb));
            }

            LOG(tcb, "oops, retry to send FIN");
        }
        LOG(tcb, "Oops, syn failed");
        return -1;
    }
}

/**
 * 这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
 * 如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
 * 当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
 */
void *sendbuf_timer(void *arg)
{
    client_tcb_t *tcb = arg;
    LOG(tcb, "sendbuf_timer started");
    for (;;) {
        struct timeval tv = ns_to_tv(SENDBUF_POLLING_INTERVAL);
        select(0, NULL, NULL, NULL, &tv);

        pthread_mutex_lock(tcb->bufMutex);
        segBuf_t *curr = tcb->sendBufHead;
        for (; curr != tcb->sendBufunSent; curr = curr->next) {
            // The time out for a data segment is smaller than the SENDBUF_POLLING_INTERVAL.
            // Therefore we can assume that these data hasn't been acked.
            LOG(tcb, "resends seq %d", curr->seg.header.seq_num);
            sip_sendseg(son_connection, &curr->seg);
        }
        for (; curr != NULL; curr = curr->next) {
            sip_sendseg(son_connection, &curr->seg);
            tcb->sendBufunSent = tcb->sendBufunSent->next;
        }
        if (tcb->sendBufHead == NULL) {
            LOG(tcb, "send buffer timer exits");
            tcb->sendBufTail = NULL;
            pthread_mutex_unlock(tcb->bufMutex);
            return arg;
        }
        pthread_mutex_unlock(tcb->bufMutex);
    }
    return arg;
}

/**
 * @brief 发送数据给STCP服务器
 *
 * 这个函数使用套接字ID找到TCB表中的条目.
 * 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
 * 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
 * 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生.
 * 这个函数在成功时返回1，否则返回-1.
 */
int stcp_client_send(int sockfd, void *data, unsigned int length)
{
    client_tcb_t *tcb = tcbs[sockfd];
    if (tcb == NULL) {
        log(RED "The socket %d is invalid" NORMAL, sockfd);
        return -1;
    } else if (tcb->state != CONNECTED) {
        LOG(tcb, "is under %s, and cannot send data", state_to_s(tcb));
        return -1;
    }

    unsigned int rest_len = 0;
    char *rest_data = NULL;
    if (length > MAX_SEG_LEN) {
        rest_len = length - MAX_SEG_LEN;
        rest_data = (char *)data + MAX_SEG_LEN;
        length = MAX_SEG_LEN;
    }

    // Send buffer can be set up without locking.
    segBuf_t *sendbuf = calloc(1, sizeof(*sendbuf));
    sendbuf->next = NULL;
    sendbuf->sentTime = tcb->send_time;
    sendbuf->seg.header.type = DATA;
    sendbuf->seg.header.src_port = tcb->client_portNum;
    sendbuf->seg.header.dest_port = tcb->server_portNum;
    sendbuf->seg.header.length = length;
    sendbuf->seg.header.seq_num = tcb->next_seqNum;
    tcb->next_seqNum += length;
    memcpy(sendbuf->seg.data, data, length);

    checksum(&sendbuf->seg);

    pthread_mutex_lock(tcb->bufMutex);
    /* TODO The condition varialbe on window size is buggy
    while (tcb->unAck_segNum == GBN_WINDOW) {
        LOG(tcb, "wait window clear");
        pthread_cond_wait(tcb->bufCond, tcb->bufMutex);
        LOG(tcb, "wake up");
    }
     */
    LOG(tcb, "adds send buffer under window size %d", tcb->unAck_segNum);
    tcb->unAck_segNum++;
    if (tcb->sendBufTail == NULL) {
        LOG(tcb, "This is the first send bufferd");
        assert(tcb->sendBufHead == NULL);  // This causes tail to be NULL.
        assert(tcb->sendBufunSent == NULL);  // This one runs into NULL at first.
        tcb->sendBufTail = sendbuf;
        tcb->sendBufHead = tcb->sendBufTail;
        tcb->sendBufunSent = tcb->sendBufHead;
        pthread_t tid;
        pthread_create(&tid, NULL, sendbuf_timer, tcb);
    } else {
        tcb->sendBufTail->next = sendbuf;
        tcb->sendBufTail = sendbuf;
        if (tcb->sendBufunSent == NULL) {
            LOG(tcb, "Send buffers are all sent but not acked yet");
            assert(tcb->sendBufHead);
            tcb->sendBufunSent = sendbuf;
        }
    }
    pthread_mutex_unlock(tcb->bufMutex);

    if (rest_len != 0) {
        return stcp_client_send(sockfd, rest_data, rest_len);
    } else {
        return 1;
    }
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

    if (tcb == NULL) {
        log("Invalid stcp socket %d", sockfd);
        return 0;
    } else if (tcb->state != CONNECTED) {
        log("Socket %d is to be disconnected but under %s",
            sockfd, client_state_s[tcb->state]);
        return 0;
    } else {
        while (tcb->sendBufHead != NULL) {}
        tcb->state = FINWAIT;
        LOG(tcb, "shifts into %s", state_to_s(tcb));

        //设置等待时间
        for (int i = 0; i < FIN_MAX_RETRY; i++) {
            if (send_ctrl(FIN, tcb->client_portNum, tcb->server_portNum) == -1) {
                // 连接断开，直接退出。
                tcb->state = CLOSED;
                break;
            }

            tcb->timeout.tv_sec = FIN_TIMEOUT / 1000000000;
            tcb->timeout.tv_usec = (FIN_TIMEOUT % 1000000000) / 1000000;
            tcb->is_time_out = 0;

            pthread_t tid;
            pthread_create(&tid, NULL, timer, tcb);

            while (tcb->state != CLOSED && !tcb->is_time_out) {}
            if (tcb->state == CLOSED) {
                LOG(tcb, "Socket %d shifts into %s", sockfd, state_to_s(tcb));
                return 0;
            } else {
                LOG(tcb, "%s time out", state_to_s(tcb));
            }

            LOG(tcb, "oops, retry to send FIN");
        }

        log("Oops, failed to disconnect");
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
    LOG(tcb, "is to be closed");
    tcbs[sockfd] = NULL;

    pthread_mutex_lock(tcb->bufMutex);
    while (tcb->sendBufHead) {
        segBuf_t *tmp = tcb->sendBufHead;
        tcb->sendBufHead = tcb->sendBufHead->next;
        free(tmp);
    }
    tcb->sendBufHead = NULL;
    tcb->sendBufTail = NULL;
    tcb->sendBufunSent = NULL;
    pthread_mutex_unlock(tcb->bufMutex);

    free(tcb->bufMutex);
    free(tcb->bufCond);
    // We do not need to free sendBufTail and sendBufUnsent,
    // as they should aside on the linked list started from starting from sendBufHead.
    free(tcb);

    return 0;
}

/**
 * @brief Modify send buffer list according to the DATAACK segment.
 * @param tcb The tcb to which the send buffer list belongs.
 * @param seg The DATAACK segment.
 *
 * Release all of the send buffers whose sequence number (a.k.a. the starting byte index) is less than
 * the DATAACK's sequence number (a.k.a. the expected sequence from server)
 */
static void handle_dataack(client_tcb_t *tcb, seg_t *seg)
{
    // TODO Check seq
    assert(seg->header.type == DATAACK);
    pthread_mutex_lock(tcb->bufMutex);
    while (tcb->sendBufHead != tcb->sendBufunSent) {
        if (tcb->sendBufHead->seg.header.seq_num < seg->header.seq_num) {
            segBuf_t *tmp = tcb->sendBufHead;
            LOG(tcb, "release acked send buffer (seq num %d)", tmp->seg.header.seq_num);
            tcb->sendBufHead = tcb->sendBufHead->next;
            tcb->unAck_segNum--;
            free(tmp);
        } else {
            break;
        }
    }
    if (tcb->unAck_segNum < GBN_WINDOW) {
        pthread_cond_signal(tcb->bufCond);
    }
    pthread_mutex_unlock(tcb->bufMutex);
}

//客户端状态机
static void client_fsm(client_tcb_t *tcb, seg_t *seg)
{
    switch (tcb->state) {
    case CLOSED:
        log("Unexpected %s state", client_state_s[CLOSED]);
        break;
    case SYNSENT:
        switch (seg->header.type) {
        case SYNACK:
            tcb->state = CONNECTED;
            LOG(tcb, "enters %s state", state_to_s(tcb));
            break;
        default:
            LOG(tcb, "receives unexpect %s segment under %s",
                seg_type_s(seg), client_state_s[SYNSENT]);
        }
        break;
    case CONNECTED:
        switch (seg->header.type) {
        case DATAACK:
            handle_dataack(tcb, seg);
            break;
        case SYNACK:
            break;
        default:
            LOG(tcb, "receives unexpected segment");
            assert(0);
        }
        break;
    case FINWAIT:
        switch (seg->header.type) {
        case DATAACK:
            // As stcp_client_send() is non-blocking, so the client may immediately get into FINWAIT state.
            // This state won't stay long enough to handle all DATAACK in a high missing rate.
            // If DATAACK segment is missed and a FINACK arrives, the client will release the send buffers,
            // which may result in unexpected behaviors.
            handle_dataack(tcb, seg);
            break;
        case FINACK:
            tcb->state = CLOSED;
            LOG(tcb, "returns %s", client_state_s[CLOSED]);
            break;
        default:
            LOG(tcb, "receives unexpect %s segment under %s",
                seg_type_s(seg), client_state_s[FINWAIT]);
        }
        break;
    default:
        LOG(tcb, "unexpect tcb state");
    }
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段.
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.

void *seghandler(void* arg)
{
    for (;;) {
        seg_t seg = {};
        int result = sip_recvseg(son_connection, &seg);
        if (result == -1) {
            // 收到了模拟 SON 的 TCP 的断开连接请求。
            log("SON closed");
            son_connection = -1;
            break;
        } else if (result == 1) {
            // 丢包
            log(RED "Oops, missing " NORMAL "%s" RED " from %d to %d" NORMAL,
                seg_type_s(&seg), seg.header.src_port, seg.header.dest_port);
            continue;
        } else if (result == 2) {
            //段损坏
            log(RED "Oops, polluted " NORMAL);
            continue;
        }

        log(">>> Receive %s segment from %d to %d",
            seg_type_s(&seg), seg.header.src_port, seg.header.dest_port);

        for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
            if (tcbs[i] && tcbs[i]->client_portNum == seg.header.dest_port) {
                client_fsm(tcbs[i], &seg);
                break;  // efficiency!
            }
        }

        log("<<< Packet handling done");
    }

    log("Seghander exits");
    return arg;
}
