//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h> // 定时信号

#include "../common/constants.h"
#include "../common/seg.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON()
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror(NULL);
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error");
        return -1;
    }

    puts("unix domain established");

    return fd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.
//路由更新报文包含这个节点的距离矢量.
//广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
static void *routeupdate_daemon(void *arg)
{
    //你需要编写这里的代码.
    struct timeval tv;
    tv.tv_sec = ROUTEUPDATE_INTERVAL;
    tv.tv_usec = 0;

    pkt_routeupdate_t update;
    memset(&update, 0, sizeof(update));
    update.entryNum = 0;

    sip_pkt_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.header.type = ROUTE_UPDATE;
    pkt.header.length = sizeof(update);
    pkt.header.src_nodeID = topology_getMyNodeID();
    pkt.header.dest_nodeID = 0;
    memcpy(pkt.data, &update, sizeof(update));

    while (1) {
        select(0, NULL, NULL, NULL, &tv);
        //son_sendpkt(BROADCAST_NODEID, &pkt, son_conn);
    }

    return NULL;
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void *pkthandler(void *arg)
{
    puts("pkt handler starts");
    sip_pkt_t pkt;
    while (son_recvpkt(&pkt, son_conn) > 0) {
        printf("Routing: received a packet from neighbor %d\n", pkt.header.src_nodeID);
        if (pkt.header.type == ROUTE_UPDATE) {
            puts("route update!");
        } else if (topology_getMyNodeID() == pkt.header.dest_nodeID) {  // TODO save my id
            printf("recv segment from %d\n", pkt.header.src_nodeID);
            // 转发给 STCP 不检查返回值是因为可以允许连续若干个 STCP 用例，所以中途断开可以被容忍。
            forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, (void *)&pkt.data);
        } else {
            int next_id = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
            printf("foward: seg(%d -> %d) next hop %d\n", pkt.header.src_nodeID, pkt.header.dest_nodeID, next_id);
            if (son_sendpkt(next_id, &pkt, son_conn) < 0) {
                break; // 不可接受 SON 的异常
            }
        }
    }

    shutdown(son_conn, SHUT_RDWR);
    close(son_conn);
    son_conn = -1;
    puts("pkt handler exits");
    return 0;
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数.
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop(int unused)
{
    shutdown(son_conn, SHUT_RDWR);
    close(son_conn);
    exit(0);
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t.
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
static void waitSTCP()
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror(NULL);
    }

    struct sockaddr_un sockaddr_un;
    memset(&sockaddr_un, 0, sizeof(sockaddr_un));
    sockaddr_un.sun_family = AF_UNIX;
    strncpy(sockaddr_un.sun_path, STCP_PATH, sizeof(sockaddr_un.sun_path) - 1);
    unlink(STCP_PATH);
    if (bind(fd, (struct sockaddr *)&sockaddr_un, sizeof(sockaddr_un)) == -1) {
        perror(NULL);
    }
    if (listen(fd, 5) == -1) {
        perror(NULL);
    }

    // 无限循环以接受任意多次 STCP 连接，但是一次只支持一个。
    // 本函数位于 main 的末尾，所以程序不会从这里退出，只能通过 SIGINT 退出
    for (;;) {
        stcp_conn = accept(fd, NULL, NULL);
        if (stcp_conn == -1) {
            perror(NULL);
            continue;
        } else {
            puts("unix domain for sip-stcp established");
        }

        int dst_id;
        sip_pkt_t pkt;
        seg_t *segptr = (void *)pkt.data;  // 直接往 data 段里写，减少一次结构体拷贝
        while (getsegToSend(stcp_conn, &dst_id, segptr) > 0) {
            // 初始路由
            int next_id = routingtable_getnextnode(routingtable, dst_id);
            printf("stcp segment to %d, forwarding to %d\n", dst_id, next_id);
            // 准备网络层协议头，按有效数据长度标记长度并拷贝数据
            pkt.header.dest_nodeID = dst_id;
            pkt.header.src_nodeID = topology_getMyNodeID();
            pkt.header.length = sizeof(segptr->header) + segptr->header.length;
            pkt.header.type = SIP;
            if (son_sendpkt(next_id, &pkt, son_conn) < 0) {
                return;  // 不可接受 SON 的异常
            } else {
                puts("send pkt successfully");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    printf("SIP layer is starting, pls wait...\n");

    //初始化全局变量
    nct = nbrcosttable_create();
    dv = dvtable_create();
    dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(dv_mutex,NULL);
    routingtable = routingtable_create();
    routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(routingtable_mutex,NULL);
    son_conn = -1;
    stcp_conn = -1;

    nbrcosttable_print(nct);
    dvtable_print(dv);
    routingtable_print(routingtable);

    //注册用于终止进程的信号句柄
    signal(SIGINT, sip_stop);

    //连接到本地SON进程
    son_conn = connectToSON();
    if(son_conn<0) {
        printf("can't connect to SON process\n");
        exit(1);
    }

    //启动线程处理来自SON进程的进入报文
    pthread_t pkt_handler_thread;
    pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

    //启动路由更新线程
    pthread_t routeupdate_thread;
    pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);

    printf("SIP layer is started...\n");
    printf("waiting for routes to be established\n");
    //sleep(SIP_WAITTIME);
    routingtable_print(routingtable);

    //等待来自STCP进程的连接
    printf("waiting for connection from STCP process\n");
    waitSTCP();

}
