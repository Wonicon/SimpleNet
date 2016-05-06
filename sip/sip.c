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
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn;  //到重叠网络的连接

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

//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居,
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播

static void *routeupdate_daemon(void *arg)
{
    //你需要编写这里的代码.
    struct timeval tv;
    tv.tv_sec = ROUTE_UPDATE;
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
        son_sendpkt(BROADCAST_NODEID, &pkt, son_conn);
    }

    return NULL;
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数.
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop()
{
    //你需要编写这里的代码.
}

int main(int argc, char *argv[])
{
    printf("SIP layer is starting, please wait...\n");

    //初始化全局变量
    son_conn = -1;

    //注册用于终止进程的信号句柄
    signal(SIGINT, sip_stop);

    //连接到重叠网络层SON
    son_conn = connectToSON();
    if (son_conn < 0) {
        printf("can't connect to SON process\n");
        exit(1);
    }

    //启动路由更新线程

    pthread_t tid;
    pthread_create(&tid, NULL, routeupdate_daemon, NULL);

    printf("SIP layer is started...\n");

    puts("pkt handler starts");
    sip_pkt_t pkt;
    while (son_recvpkt(&pkt, son_conn) > 0) {
        printf("Routing: received a packet from neighbor %d\n", pkt.header.src_nodeID);
    }
    close(son_conn);
    son_conn = -1;
    puts("pkt handler exits");
}


