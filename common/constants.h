//文件名: common/constants.h

//创建日期: 2015年

#ifndef CONSTANTS_H
#define CONSTANTS_H

//服务器打开的重叠网络层端口号. 客户端将连接到这个端口. 你应该选择一个随机的端口以避免和其他同学发生冲突.
#define SON_PORT 9009
//这是STCP可以支持的最大连接数. 你的TCB表应包含MAX_TRANSPORT_CONNECTIONS个条目.
#define MAX_TRANSPORT_CONNECTIONS 10
//最大段长度
//MAX_SEG_LEN = 1500 - sizeof(stcp header) - sizeof(sip header)
#define MAX_SEG_LEN  1464
//数据包丢失率为10%
#define PKT_LOSS_RATE 0.1
//SYN_TIMEOUT值, 单位为纳秒
#define SYN_TIMEOUT 1000000000
//FIN_TIMEOUT值, 单位为纳秒
#define FIN_TIMEOUT 1000000000
//stcp_client_connect()中的最大SYN重传次数
#define SYN_MAX_RETRY 5
//stcp_client_disconnect()中的最大FIN重传次数
#define FIN_MAX_RETRY 5
//服务器CLOSEWAIT超时值, 单位为秒
#define CLOSEWAIT_TIMEOUT 10
//stcp_server_accept()函数使用这个时间间隔来忙等待TCB状态转换, 单位为纳秒
#define ACCEPT_POLLING_INTERVAL 100000000
//sendBuf_timer线程的轮询间隔, 单位为纳秒
#define SENDBUF_POLLING_INTERVAL 100000000
//STCP客户端在stcp_server_recv()函数中使用这个时间间隔来轮询接收缓冲区, 以检查是否请求的数据已全部到达, 单位为秒.
#define RECVBUF_POLLING_INTERVAL 1
//接收缓冲区大小
#define RECEIVE_BUF_SIZE 1000000
//数据段超时值, 单位为纳秒
#define DATA_TIMEOUT 100000000
//GBN窗口大小
#define GBN_WINDOW 10

#include <sys/types.h>
/**
 * @brief Convert nanoseconds to timeval.
 */
static inline struct timeval ns_to_tv(int ns)
{
#define SEC_IN_NS  1000000000
#define USEC_IN_NS 1000
    struct timeval tv;
    tv.tv_sec = ns / SEC_IN_NS;
    tv.tv_usec = (ns % SEC_IN_NS) / USEC_IN_NS;
    return tv;
}

/*******************************************************************/
//重叠网络参数
/*******************************************************************/

//这个端口号用于重叠网络中节点之间的互联, 你应该修改它为一个随机值以避免和其他同学的设置发生冲突
#define CONNECTION_PORT 3490

//最大SIP报文数据长度: 1500 - sizeof(sip header)
#define MAX_PKT_LEN 1488

/*******************************************************************/
//网络层参数
/*******************************************************************/

//用于 UNIX DOMAIN 通信的 PATH
//这是一个隐藏符号链接
#define UNIX_PATH "son-sip"
#define STCP_PATH "sip-stcp"

//最大路由表槽数 
#define MAX_ROUTINGTABLE_SLOTS 10

//重叠网络支持的最大节点数
#define MAX_NODE_NUM 10

//无穷大的链路代价值, 如果两个节点断开连接了, 它们之间的链路代价值就是INFINITE_COST
#define INFINITE_COST 999

//这是广播节点ID. 如果SON进程从SIP进程处接收到一个目标节点ID为BROADCAST_NODEID的报文, 它应该将该报文发送给它的所有邻居
#define BROADCAST_NODEID 9999

//路由更新广播间隔, 以秒为单位
#define ROUTEUPDATE_INTERVAL 2

#endif
