//文件名: son/son.c
//
//描述: 这个文件实现SON进程
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程.
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中.
//
//创建日期: 2015年

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>  // UNIX DOMAIN 接口
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include "common.h"
#include "constants.h"
#include "pkt.h"
#include "son.h"
#include "../topology/topology.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 1

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量
nbr_entry_t *nt;

//记录所有监听线程的 tid
pthread_t *tids;

//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止.
void *waitNbrs(void *arg)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Cannot open socket");
        pthread_exit(NULL);
    }

    struct sockaddr_in sockaddr_in;
    sockaddr_in.sin_family      = AF_INET;
    sockaddr_in.sin_addr.s_addr = INADDR_ANY;
    sockaddr_in.sin_port        = htons(CONNECTION_PORT);

    // 使得退出后可以立即使用旧端口，方便调试
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        pthread_exit(NULL);
    }

    if (bind(fd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in))) {
        perror("Cannot bind");
        pthread_exit(NULL);
    }

    listen(fd, 5);

    nbr_entry_t *nbrs = arg;
    int nr_nbrs = topology_getNbrNum();
    int this_id = topology_getMyNodeID();

    int nr_conn = 0;
    for (int i = 0; i < nr_nbrs; i++) {
        if (nbrs[i].nodeID > this_id) {
            nr_conn++;
        }
    }

    // On `man 2 accept':
    //   The addrlen argument is a value-result argument:
    //   the caller must initialize it to contain the size (in bytes) of the structure pointed to by addr;
    //   on return it will contain the actual size of the peer address.
    // 启发: http://stackoverflow.com/questions/32054055/why-does-it-show-received-a-connection-from-0-0-0-0-port-0
    socklen_t len = sizeof(sockaddr_in);
    while (nr_conn--) {
        int conn = accept(fd, (struct sockaddr *)&sockaddr_in, &len);
        if (conn == -1) {
            perror("Cannot connect to the neighbor");
        }
        else {
            int id = topology_getNodeIDfromip(&sockaddr_in.sin_addr);
            for (int i = 0; i < nr_nbrs; i++) {
                if (nbrs[i].nodeID == id) {
                    log("%d is connected to %d", this_id, id);
                    nbrs[i].conn = conn;
                    break;
                }
            }
        }
    }

    return 0;
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs()
{
    int nr_nbrs = topology_getNbrNum();
    int this_id = topology_getMyNodeID();

    for (int i = 0; i < nr_nbrs; i++) {
        if (this_id > nt[i].nodeID) {
            struct sockaddr_in sockaddr_in;
            sockaddr_in.sin_family = AF_INET;
            sockaddr_in.sin_addr.s_addr = ntohl(nt[i].nodeIP);
            sockaddr_in.sin_port = ntohs(CONNECTION_PORT);
            nt[i].conn = socket(AF_INET, SOCK_STREAM, 0);

            if (nt[i].conn == -1) {
                perror("Cannot create the client socket");
                exit(-1);
            }

            // 使得退出后可以立即使用旧端口，方便调试
            int enable = 1;
            if (setsockopt(nt[i].conn, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
                perror("setsockopt SO_REUSEADDR");
            }

            log("%d is connecting to %d", this_id, nt[i].nodeID);
            while (connect(nt[i].conn, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in))) {
                perror("Cannot connect to the neighbor");
                log("Retry");
            }
            log("%d is connected to %d", this_id, nt[i].nodeID);
        }
    }
    return 0;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
// arg: 指向 nbr_entry_t 的指针, 引用需要处理的邻居
void *listen_to_neighbor(void *arg)
{
    volatile nbr_entry_t *nbr = arg;

    log("Listening on %d", nbr->nodeID);

    sip_pkt_t sip_pkt;
    for (;;) {
        int ret = recvpkt(&sip_pkt, nbr->conn);
        if (ret == -2) {
            break;
        } else if (ret != -1) {
            log("Received a pkt from %d", sip_pkt.header.src_nodeID);
            if (forwardpktToSIP(&sip_pkt, sip_conn) == -1) {
                warn("Forwarding to SIP failed");
            }
        } else {
            warn("pkt damage? check semantic collision with the markup!");
        }
    }

    log("Exit listener on %d", nbr->nodeID);
    return NULL;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接.
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳.
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP()
{
    int unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_socket == -1) {
        perror(NULL);
    }

    struct sockaddr_un sockaddr_un;
    memset(&sockaddr_un, 0, sizeof(sockaddr_un));
    sockaddr_un.sun_family = AF_UNIX;
    strncpy(sockaddr_un.sun_path, UNIX_PATH, sizeof(sockaddr_un.sun_path) - 1);
    unlink(UNIX_PATH);
    if (bind(unix_socket, (struct sockaddr *)&sockaddr_un, sizeof(sockaddr_un)) == -1) {
        perror(NULL);
    }
    if (listen(unix_socket, 5) == -1) {
        perror(NULL);
    }
    if ((sip_conn = accept(unix_socket, NULL, NULL)) == -1) {
        perror(NULL);
    }

    log("unix domain established");
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop(int unused)
{
    int nr_nbrs = topology_getNbrNum();
    nt_destroy(nt);
    for (int i = 0; i < nr_nbrs; i++) {
        pthread_join(tids[i], NULL);
    }
    free(nt);
    free(tids);
    exit(1);
}

int main()
{
    //启动重叠网络初始化工作
    log("Overlay network: Node %d initializing...", topology_getMyNodeID());

    //创建一个邻居表
    nt = nt_create();
    //将sip_conn初始化为-1, 即还未与SIP进程连接
    sip_conn = -1;

    //打印所有邻居
    int nbrNum = topology_getNbrNum();
    for (int i = 0; i < nbrNum; i++) {
        log("Overlay network: neighbor %d:%d", i + 1, nt[i].nodeID);
    }

    //启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
    pthread_t waitNbrs_thread;
    pthread_create(&waitNbrs_thread, NULL, waitNbrs, nt);

    //等待其他节点启动
    //sleep(SON_START_DELAY);

    //连接到节点ID比自己小的所有邻居
    connectNbrs();

    //等待waitNbrs线程返回
    pthread_join(waitNbrs_thread, NULL);

    log("SON has been established");

    //此时, 所有与邻居之间的连接都建立好了

    //创建线程监听所有邻居
    tids = calloc((size_t)nbrNum, sizeof(*tids));
    for (int i = 0; i < nbrNum; i++) {
        pthread_create(&tids[i], NULL, listen_to_neighbor, &nt[i]);
    }
    log("Overlay network: node initialized...");
    log("Overlay network: waiting for connection from SIP process...");

    //注册一个信号句柄, 用于终止进程
    //这时SIGINT的行为才有意义
    signal(SIGINT, son_stop);

    //等待来自SIP进程的连接
    waitSIP();

    sip_pkt_t sip;
    int next_node;
    int this_id = topology_getMyNodeID();
    while (getpktToSend(&sip, &next_node, sip_conn) != -1) {
        if (next_node == BROADCAST_NODEID) {
            log("Received a broadcast");
            for (int i = 0; i < nbrNum; i++) {
                if (nt[i].nodeID != this_id) {
                    sendpkt(&sip, nt[i].conn);
                }
            }
        } else {
            int i;
            for (i = 0; i < nbrNum; i++) {
                if (next_node == nt[i].nodeID) {
                    if (sendpkt(&sip, nt[i].conn) > 0) {
                        log("send to %d successfully", next_node);
                    } else {
                        log("send to %d failed", next_node);
                    }
                    break;
                }
            }
            if (i == nbrNum) {
                log("no nbr %d found", next_node);
            }
        }
    }
}
