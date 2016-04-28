#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "stcp_server.h"
#include "common.h"

/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

/**
 * @brief 内部日志信息，自动输出 tcb 相关内容。
 */
#define LOG(tcb, fmt, ...) \
    log(MAGENTA "{tcb:%p} " NORMAL fmt, tcb, ## __VA_ARGS__)

/**
 * @brief 状态字符串
 *
 * 对应状态枚举值的字符串常量。
 */
static const char *server_state_s[] = {
#define S(x) #x
#define TOKEN(x) ORANGE S(x) NORMAL
#include "stcp_server_state.h"
#undef TOKEN
#undef S
};

/**
 * @brief 输出 tcb 的状态对应的字符串常量
 * @param tcb 传输控制块
 */
static const char *state_to_s(const server_tcb_t *tcb)
{
    return server_state_s[tcb->state];
}

/**
 * @brief TCB 池
 *
 * MAX_TRANSPORT_CONNECTIONS 是支持的最大连接数
 */
static server_tcb_t *tcbs[MAX_TRANSPORT_CONNECTIONS];

/**
 * @brief 记录 seghandler 线程的 tid
 */
pthread_t handler_tid;

/**
 * @brief 记录模拟网络层所使用的连接套接字
 */
static int son_connection;

/**
 * @brief 启动 STCP 协议栈
 * @param conn 模拟 SON 的连接套接字
 */
void stcp_server_init(int conn)
{
    // 初始化 TCB 池
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        tcbs[i] = NULL;
    }
    log("TCB pool has been initialized.");

    // 启动接受网络层报文段的线程
    son_connection = conn;
    pthread_create(&handler_tid, NULL, seghandler, NULL);
    log("Seghandler started.");
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port.
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接.
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port)
{
    if (son_connection == -1) {
        panic("son has been closed");
    }

    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        if (tcbs[i] == NULL) {
            server_tcb_t *tcb = calloc(1, sizeof(*tcb));

            tcb->server_portNum = server_port;
            tcb->client_portNum = -1;
            tcb->state = CLOSED;

            // Init mutex
            tcb->mutex = malloc(sizeof(*tcb->mutex));
            pthread_mutex_init(tcb->mutex, NULL);

            tcb->condition = malloc(sizeof(*tcb->condition));
            pthread_cond_init(tcb->condition, NULL);

            log("Assign socket %d to port %d", i, server_port);
            tcbs[i] = tcb;

            // Socket
            return i;
        }
    }

    // No socket available.
    return -1;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd)
{
    if (son_connection == -1) {
        panic("son has been closed");
    }

    server_tcb_t *tcb = tcbs[sockfd];

    if (tcb == NULL) {
        log("Invalid stcp socket %d", sockfd);
        return 0;
    }
    else if (tcb->state != CLOSED) {
        log("The state of this stcp socket is not %s", server_state_s[CLOSED]);
        return 0;
    }
    else {
        pthread_mutex_lock(tcb->mutex);
        tcb->state = LISTENING;
        pthread_mutex_unlock(tcb->mutex);

        LOG(tcb, "shifts state to %s", state_to_s(tcb));

        // 等待 seghandler() 唤醒
        pthread_mutex_lock(tcb->mutex);
        if (tcb->state != CONNECTED) {
            pthread_cond_wait(tcb->condition, tcb->mutex);
        }
        pthread_mutex_unlock(tcb->mutex);

        LOG(tcb, "establishes connection");
        return 1;
    }
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
    return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

/**
 * @brief Release the tcb resource after a timeout
 * @param arg the POINTER to the specific tcb entry that we want to release.
 *
 * To make the tcb entry NULL again, we should pass &tcb[i] to this thread.
 */
static void *closewait_handler(void *arg)
{
    server_tcb_t **tcbs_entry = arg;
    server_tcb_t *tcb = *tcbs_entry;

    struct timeval tv = { .tv_sec = CLOSEWAIT_TIMEOUT };
    if (select(0, NULL, NULL, NULL, &tv) < 0) {
        perror("select");
    }

    LOG(*tcbs_entry, "closewait timeout");

    pthread_mutex_lock(tcb->mutex);
    // Note the fsm handler thread may be locked by this mutex.
    // The tcb must be on CLOSEWAIT. Currently it does not need the tcb pointer to be valid except for logging.
    // At least now we can set tcbs_entry to NULL to avoid any new duplicated packet being handled.
    *tcbs_entry = NULL;
    pthread_mutex_unlock(tcb->mutex);

    free(tcb->mutex);
    free(tcb->condition);
    if (tcb->recvBuf) {
        free(tcb->recvBuf);
    }
    free(tcb);

    return arg;
}

int stcp_server_close(int sockfd)
{
    if (son_connection == -1) {
        panic("son has been closed");
    }

    server_tcb_t *tcb = tcbs[sockfd];
    log("Socket listening on port %d is to be closed", tcb->server_portNum);
    log("waiting connection %d getting into %s", sockfd, server_state_s[CLOSEWAIT]);

    pthread_mutex_lock(tcb->mutex);
    if (tcb->state != CLOSEWAIT) {
        pthread_cond_wait(tcb->condition, tcb->mutex);
    }
    pthread_mutex_unlock(tcb->mutex);

    LOG(tcb, "connection %d getting into %s", sockfd, state_to_s(tcb));

    pthread_t tid;
    pthread_create(&tid, NULL, closewait_handler, &tcbs[sockfd]);
    return 0;
}

/**
 * @brief 发送控制报文
 */
static inline void send_ctrl(unsigned short type, unsigned short src_port, unsigned short dst_port)
{
    seg_t synack = {
        .header.src_port = src_port,
        .header.dest_port = dst_port,
        .header.length = 0,
        .header.type = type,
    };
    if (sip_sendseg(son_connection, &synack) == -1) {
        log("sending ctrl to port %d failed", dst_port);
    }
}

/**
 * @brief TCB 状态机
 */
static void server_fsm(server_tcb_t *tcb, seg_t *seg)
{
    switch (tcb->state) {
    case CLOSED:
        LOG(tcb, "unexpected state %s", state_to_s(tcb));
        break;
    case LISTENING:
        switch (seg->header.type) {
        case SYN:
            send_ctrl(SYNACK, seg->header.dest_port, seg->header.src_port);
            LOG(tcb, "has sent %s", seg_type_s(seg));

            // 这里上锁似乎没有什么作用 !?
            pthread_mutex_lock(tcb->mutex);
            tcb->state = CONNECTED;
            pthread_cond_signal(tcb->condition);
            pthread_mutex_unlock(tcb->mutex);

            LOG(tcb, "enters state %s", state_to_s(tcb));
            break;
        default:
            LOG(tcb, "unexpected %s segment for state %s", seg_type_s(seg), state_to_s(tcb));
        }
        break;
    case CONNECTED:
        switch (seg->header.type) {
        case SYN:
            send_ctrl(SYNACK, seg->header.dest_port, seg->header.src_port);
            LOG(tcb, "receives duplicated SYN request");
            break;
        case FIN:
            send_ctrl(FINACK, seg->header.dest_port, seg->header.src_port);
            LOG(tcb, "has sent %s", seg_type_s(seg));

            // 这里上锁似乎没有什么作用 !?
            pthread_mutex_lock(tcb->mutex);
            tcb->state = CLOSEWAIT;
            pthread_cond_signal(tcb->condition);
            pthread_mutex_unlock(tcb->mutex);

            LOG(tcb, "enters state %s", state_to_s(tcb));
            break;
        }
        break;
    case CLOSEWAIT:
        switch (seg->header.type) {
        case FIN:
            send_ctrl(FINACK, seg->header.dest_port, seg->header.src_port);
            LOG(tcb, "receives duplicated %s segment", seg_type_s(seg));
            break;
        default:
            // Replacing the state symbol from a dynamic state_to_s call to a static expression keeps the coupling
            // of the symbol and its literal string but avoid potential hazard on accessing a dangling pointer.
            // Because the closewait_handler will free the pointer.
            LOG(tcb, "unexpected %s segment for state %s", seg_type_s(seg), server_state_s[CLOSEWAIT]);
        }
        break;
    default:
        log("Unexpected tcb state %d", tcb->state);
    }
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环,
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
void *seghandler(void* arg)
{
    for (;;) {
        seg_t seg = {};
        int result = sip_recvseg(son_connection, &seg);
        if (result == -1) {
            // 收到了模拟 SON 的 TCP 的断开连接请求。
            log("SON closed");
            son_connection = -1;

            // Notice all valid tcb, avoid infinite stalling.
            log("Wake up all waiting api-users");
            for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
                if (tcbs[i]) {
                    log("Sadly, socket %d has not been closed", i);
                    pthread_cond_signal(tcbs[i]->condition);
                }
            }

            break;
        }
        else if (result == 1) {
            // 丢包
            log(RED "Oops, missing " NORMAL "%s" RED " from %d to %d" NORMAL,
                    seg_type_s(&seg), seg.header.src_port, seg.header.dest_port);
            continue;
        }

        log(">>> Receive %s segment from %d to %d",
                seg_type_s(&seg), seg.header.src_port, seg.header.dest_port);

        // Search & forward.
        // TODO non-block!!!
        for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
            if (tcbs[i] && tcbs[i]->server_portNum == seg.header.dest_port) {
                log("forward the packet to tcb %p", tcbs[i]);
                server_fsm(tcbs[i], &seg);
                break;  // efficiency!
            }
        }

        log("<<< Packet handling done");

    }

    log("Seghandler exits");
    return arg;
}
