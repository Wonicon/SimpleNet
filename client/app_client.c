//文件名: client/app_client.c
//
//描述: 这是客户端应用程序代码. 客户端首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层. 然后它调用stcp_client_init()初始化STCP客户端.
//它通过两次调用stcp_client_sock()和stcp_client_connect()创建两个套接字并连接到服务器. 经过一段时间后, 客户端调用stcp_client_disconnect()断开到服务器的连接.
//最后,客户端调用stcp_client_close()关闭套接字. 重叠网络层通过调用son_stop()停止.

//创建日期: 2015年

//输入: 无

//输出: STCP客户端状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include "../common/constants.h"
#include "../common/common.h"
#include "stcp_client.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在连接创建后, 等待5秒, 然后关闭连接
#define WAITTIME 5

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1.

static short son_port = 0;

int son_start(char *serv_ip)
{
    //创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        sys_panic("socket");
    }

    struct sockaddr_in serv_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = inet_addr(serv_ip),
        .sin_port        = htons(son_port),
    };

    if(connect(fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
        sys_panic("connect");

    return fd;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn)
{
    shutdown(son_conn, SHUT_RDWR);
    close(son_conn);
}

static int son_conn = -1;

static inline void Exit(int state)
{
    son_stop(son_conn);
    exit(state);
}

int main(int argc, char *argv[])
{
    //用于丢包率的随机数种子
    srand(time(NULL));

    if(argc < 3) {
        son_port = SON_PORT;
    }
    else {
        int port = atoi(argv[2]);
        if(!(port > 0 && port < SHRT_MAX)) {
            panic("%d exceeds short limit", port);
        }
        else {
            son_port = (short)port;
        }
    }

    printf("start son on port %d\n", son_port);

    //启动重叠网络层并获取重叠网络层TCP套接字描述符
    son_conn = son_start(argv[1]);
    if(son_conn < 0) {
        printf("fail to start overlay network\n");
        Exit(1);
    }

    //初始化stcp客户端, 传递TCP套接字描述符给STCP层, STCP将该套接字作为一个全局变量维护
    stcp_client_init(son_conn);

    sleep(1);

    //在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
    int sockfd = stcp_client_sock(CLIENTPORT1);
    if(sockfd < 0) {
        printf("fail to create stcp client sock");
        Exit(1);
    }
    if(stcp_client_connect(sockfd, SERVERPORT1) < 0) {
        printf("fail to connect to stcp server\n");
        Exit(1);
    }

    log("client (port %d) connect to server (port %d)", CLIENTPORT1, SERVERPORT1);

    //在端口89上创建STCP客户端套接字, 并连接到STCP服务器端口90
    int sockfd2 = stcp_client_sock(CLIENTPORT2);
    if(sockfd2 < 0) {
        printf("fail to create stcp client sock");
        Exit(1);
    }
    if(stcp_client_connect(sockfd2, SERVERPORT2) < 0) {
        printf("fail to connect to stcp server\n");
        Exit(1);
    }

    log("client (port %d) connect to server (port %d)", CLIENTPORT2, SERVERPORT2);

    //等待一段时间, 然后关闭连接
    sleep(WAITTIME);

    if(stcp_client_disconnect(sockfd) < 0) {
        printf("fail to disconnect from stcp server\n");
        Exit(1);
    }
    if(stcp_client_close(sockfd) < 0) {
        printf("failt to close stcp client\n");
        Exit(1);
    }

    if(stcp_client_disconnect(sockfd2) < 0) {
        printf("fail to disconnect from stcp server\n");
        Exit(1);
    }
    if(stcp_client_close(sockfd2) < 0) {
        printf("failt to close stcp client\n");
        Exit(1);
    }

    sleep(WAITTIME);

    //停止重叠网络层
    son_stop(son_conn);
}
