//文件名: server/app_simple_server.c

//描述: 这是简单版本的服务器程序代码. 服务器首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层. 然后它调用stcp_server_init()初始化STCP服务器. 
//它通过两次调用stcp_server_sock()和stcp_server_accept()创建2个套接字并等待来自客户端的连接. 服务器然后接收来自两个连接的客户端发送的短字符串. 
//最后, 服务器通过调用stcp_server_close()关闭套接字. 重叠网络层通过调用son_stop()停止.

//创建日期: 2015年

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "constants.h"
#include "common.h"
#include "stcp_server.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在接收到字符串后, 等待10秒, 然后关闭连接.
#define WAITTIME 10

static short son_port = 0;

/**
 * @brief 创建与客户的 TCP 连接作为底层网络
 * @return 与客户端的连接套接字
 *
 * son_start() 将像常规服务器一样，创建套接字，监听端口，等待客户连接。
 * 但是需要使用的只有连接套接字，这个 TCP 连接将作为分组交换网络（虽然是可靠交付的……）。
 *
 * 至于如何在 STCP 层创建多个连接，目前只能在客户端进程中多次建立连接实现。
 */
int son_start()
{
    // 创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);  // 监听套接字，IP 地址族，流数据（TCP）
    if (fd < 0) {
        sys_panic("socket");
    }

    // 将套接字绑定到本机 IP 地址 + 端口号。
    struct sockaddr_in serv_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(son_port),
    };

    // 使得退出后可以立即使用旧端口，方便调试
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        sys_panic("setsockopt SO_REUSEADDR");
    }

    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        sys_panic("bind");
    }

    // 监听连接。
    listen(fd, 5);

    // 只需要一个连接。
    struct sockaddr_in inaddr = {};  // useless
    socklen_t inaddr_len = 0;        // useless
    return accept(fd, (struct sockaddr *)&inaddr, &inaddr_len);
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn)
{
    // 使用 shutdown 可以让别的线程中陷入阻塞的 socket IO 直接返回。
    shutdown(son_conn, SHUT_RDWR);
    close(son_conn);
    // 在进程退出后自动关闭监听套接字。
}

int main(int argc, char *argv[]) {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//启动重叠网络层并获取重叠网络层TCP套接字描述符
    son_port = argc > 1 ? atoi(argv[1]) : SON_PORT;
	int son_conn = son_start();
	if(son_conn<0) {
		printf("can not start overlay network\n");
	}

	//初始化STCP服务器
	stcp_server_init(son_conn);

	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//接收来自第一个连接的字符串
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//接收来自第二个连接的字符串
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//停止重叠网络层
	son_stop(son_conn);
	extern pthread_t handler_tid;
	pthread_join(handler_tid, NULL);
}
