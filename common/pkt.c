/**
 * @file common/pkt.c
 */

#include "pkt.h"
#include <stdio.h>
#include <unistd.h>

enum pkt_state {
    PKTSTART1,
    PKTSTART2,
    PKTRECV,
    PKTSTOP1,
    PKTSTOP2
};

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个 unix domain socket 互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过 unix domain socket 发送给SON进程.
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t *pkt, int son_conn)
{
    sendpkt_arg_t sendptk;
    sendptk.nextNodeID = nextNodeID;
    sendptk.pkt = *pkt;
    if (write(son_conn, &sendptk, sizeof(sendptk)) > 0) {
        return 1;
    } else {
        return -1;
    }
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文.
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的 unix domain 套接字连接发送.
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t *pkt, int son_conn)
{
    if (read(son_conn, pkt, sizeof(*pkt)) > 0) {
        return 0;
    } else {
        // 连接断开或套接字销毁
        return -1;
    }
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符.
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#.
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点
// PKTSTART2 -- 接收到'!', 期待'&'
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode, int sip_conn)
{
    return 0;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的.
// SON进程调用这个函数将报文转发给SIP进程.
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符.
// 报文通过SIP进程和SON进程之间的TCP连接发送.
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t *pkt, int sip_conn)
{
    if (write(sip_conn, pkt, sizeof(*pkt)) > 0) {
        return 1;
    } else {
        return -1;
    }
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送.
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
    return 0;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#.
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点
// PKTSTART2 -- 接收到'!', 期待'&'
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收报文, 返回1, 否则返回-1.
// 断开连接返回 -2
int recvpkt(sip_pkt_t *pkt, int conn)
{
    char ch;
    char *buf = (void *)pkt;
    enum pkt_state pkt_state = PKTSTART1;
    while (pkt_state != PKTSTOP2) {
        int ret = read(conn, &ch, sizeof(ch));
        if (ret != sizeof(ch)) {
            perror("recvpkt");
            return -2;
        }
        switch (pkt_state) {
        case PKTSTART1:
            if (ch == '!') pkt_state = PKTSTART2;
            break;
        case PKTSTART2:
            if (ch == '&') pkt_state = PKTRECV;
            else return -1;
            break;
        case PKTRECV:
            if (ch == '!') pkt_state = PKTSTOP1;
            else *buf++ = ch;
            break;
        case PKTSTOP1:
            if (ch == '#') pkt_state = PKTSTOP2;
            else return -1;
            break;
        default:
            return -1;
        }
    }
    return 0;
}
