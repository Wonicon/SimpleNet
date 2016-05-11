//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程
//
//创建日期: 2015年

#include "common.h"
#include "constants.h"
#include "seg.h"
#include "pkt.h"
#include "sip.h"
#include "../topology/topology.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 5

// 邻居生存时间，在这个间隔内没有收到 ROUTE_UPDATE 报文的话，
// 就将邻居的距离矢量设置成 INFINITE_COST, 这样当其他邻居能提供次优路径时，可以被更新。
#define ALIVE_THRESHOLD 10

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
int nr_nbrs;  // 邻居结点数（一开始为了 KISS 原则，这些数据我都是用 topo 的 API 临时获取的，然而这个代码的 overhead 一点也不 KISS）
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

    log("unix domain established");

    return fd;
}

// 这个线程每隔 ALIVE_THRESHOLD 时间就检查当前的邻居表有没有
// 更新过距离矢量信息，如果有，就清空 flag，为下次检查做准备。
// 如果没有，即上次清空的 flag 没有因收到距离矢量而设置，则将邻
// 居的距离矢量设置成 INFINITE_COST，既表明链路断开，也为能够
// 更新成其他节点的路由提供条件。
static void *alive_check(void *arg)
{
    while (nr_nbrs) {
        struct timeval tv = { ALIVE_THRESHOLD, 0 };
        select(0, NULL, NULL, NULL, &tv);

        // 要上锁，不然中间这不知道什么时候就会被打断……
        log("checker awake");
        pthread_mutex_lock(dv_mutex);
        pthread_mutex_lock(routingtable_mutex);
        for (int i = 0; i < nr_nbrs; i++) {
            if (!nct[i].is_updated) {
                warn("%d is dead", nct[i].nodeID);
                nct[i].cost = INFINITE_COST;
                dvtable_setcost(dv, nct[i].nodeID, INFINITE_COST);
                // 所有以这个挂掉的邻居为 next hop 的结点的 dv 都要设置成 INFINITE_COST
                for (int j = 0; j < MAX_NODE_NUM; j++) {
                    if (dv->dvEntry[j].nodeID != -1 &&
                            routingtable_getnextnode(routingtable, dv->dvEntry[j].nodeID) == nct[i].nodeID) {
                        dv->dvEntry[j].cost = INFINITE_COST;
                    }
                }
            }
            nct[i].is_updated = 0;
        }
        pthread_mutex_unlock(routingtable_mutex);
        pthread_mutex_unlock(dv_mutex);
    }

    return NULL;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.
//路由更新报文包含这个节点的距离矢量.
//广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
static void *routeupdate_daemon(void *arg)
{
    sip_pkt_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    // 提前初始化更新报文的静态信息
    pkt.header.type = ROUTE_UPDATE;
    // TODO 这里为了简化使用了数组，所以可以直接知道大小。如果改动 dvEntry 为动态数组，则这里要进行相应的改动。
    pkt.header.length = sizeof(dv->dvEntry);
    pkt.header.src_nodeID = topology_getMyNodeID();
    pkt.header.dest_nodeID = 0;

    while (1) {
        struct timeval tv;
        tv.tv_sec = ROUTEUPDATE_INTERVAL;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
        pthread_mutex_lock(dv_mutex);
        memcpy(pkt.data, dv->dvEntry, pkt.header.length);
        pthread_mutex_unlock(dv_mutex);
        log("send update pakcet...");
        if (son_sendpkt(BROADCAST_NODEID, &pkt, son_conn) < 0) {
            break;
        }
    }

    warn("daemon exits due to sip-son connection breaking");
    return NULL;
}

// 处理距离向量更新。
// 注意该函数一次只针对一个邻居（和它的小伙伴们）
void update_dv(sip_pkt_t *arg)
{
    // 更新报文只从邻居处获得，所以可以从报文头里获取需要的信息。
    // 不需要在数据段里额外传递 id 或者让 son 暴露接口。
    sip_pkt_t *pkt = arg;
    int nbr_id = pkt->header.src_nodeID;
    int this_id = topology_getMyNodeID();
    int nbr_cost = nbrcosttable_getcost(nct, nbr_id);
    dv_entry_t *nbr_dv = (void *)pkt->data;

    Assert(nbr_id != this_id, "Oops, send to self!?");

    pthread_mutex_lock(dv_mutex);
    // dv(this, node) = min { nbr_cost + dv(nbr, node) }
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (nbr_dv->nodeID != -1 && nbr_dv->nodeID != this_id) {  // A valid element
            unsigned old_dst_cost = dvtable_getcost(dv, nbr_dv->nodeID);
            log("%d -> %d -> %d: %d(old), %d(new)",
                this_id, nbr_id, nbr_dv->nodeID, old_dst_cost, nbr_cost + nbr_dv->cost);
            if (old_dst_cost > nbr_cost + nbr_dv->cost) {
                log("update dv (%d => %d) and routing table (%d => %d)", old_dst_cost, nbr_cost + nbr_dv->cost,
                    routingtable_getnextnode(routingtable, nbr_dv->nodeID), nbr_id);
                pthread_mutex_lock(routingtable_mutex);
                routingtable_setnextnode(routingtable, nbr_dv->nodeID, nbr_id);
                pthread_mutex_unlock(routingtable_mutex);
                dvtable_setcost(dv, nbr_dv->nodeID, nbr_cost + nbr_dv->cost);
            }
            else if (nbr_id == routingtable_getnextnode(routingtable, nbr_dv->nodeID)) {
                // 我们暂时无条件接受当前下一跳的距离矢量更新，以期待结点失效导致的代价提升会反馈到更远的地方，
                // 并从更宏观的场景里获得最小的距离矢量更新。
                log("update next hop's dv from %d to %d regardless", old_dst_cost, nbr_cost + nbr_dv->cost);
                dvtable_setcost(dv, nbr_dv->nodeID,
                                (nbr_cost + nbr_dv->cost) > INFINITE_COST ? INFINITE_COST : nbr_cost + nbr_dv->cost);
            }
        }
        nbr_dv++;
    }
    pthread_mutex_unlock(dv_mutex);
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void *pkthandler(void *arg)
{
    log("pkt handler starts");
    sip_pkt_t pkt;
    while (son_recvpkt(&pkt, son_conn) > 0) {
        log("Routing: received a packet from neighbor %d", pkt.header.src_nodeID);
        if (pkt.header.type == ROUTE_UPDATE) {
            log("route update!");
            Assert(pkt.header.dest_nodeID == 0, "unexpected");
            for (int i = 0; i < nr_nbrs; i++) {
                if (nct[i].nodeID == pkt.header.src_nodeID) {
                    log("%d is alive", nct[i].nodeID);
                    nct[i].is_updated = 1;
                }
            }
            update_dv(&pkt);
        }
        else if (topology_getMyNodeID() == pkt.header.dest_nodeID) {  // TODO save my id
            log("recv segment from %d", pkt.header.src_nodeID);
            // 转发给 STCP 不检查返回值是因为可以允许连续若干个 STCP 用例，所以中途断开可以被容忍。
            if (forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, (void *)&pkt.data) > 0) {
                log("forward to stcp successfully");
            } else {
                warn("forwarding to stcp failed");
            }
        }
        else {
            pthread_mutex_lock(routingtable_mutex);
            int next_id = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
            pthread_mutex_unlock(routingtable_mutex);
            log("forward: seg(%d -> %d) next hop %d", pkt.header.src_nodeID, pkt.header.dest_nodeID, next_id);
            if (son_sendpkt(next_id, &pkt, son_conn) < 0) {
                break; // 不可接受 SON 的异常
            }
        }
    }

    shutdown(son_conn, SHUT_RDWR);
    close(son_conn);
    son_conn = -1;
    log("pkt handler exits");
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
            log("unix domain for sip-stcp established");
        }

        int dst_id;
        sip_pkt_t pkt;
        seg_t *segptr = (void *)pkt.data;  // 直接往 data 段里写，减少一次结构体拷贝
        while (getsegToSend(stcp_conn, &dst_id, segptr) > 0) {
            // 初始路由
            pthread_mutex_lock(routingtable_mutex);
            int next_id = routingtable_getnextnode(routingtable, dst_id);
            pthread_mutex_unlock(routingtable_mutex);
            if (next_id != -1) {
                log("stcp segment to %d, forwarding to %d", dst_id, next_id);
                // 准备网络层协议头，按有效数据长度标记长度并拷贝数据
                pkt.header.dest_nodeID = dst_id;
                pkt.header.src_nodeID = topology_getMyNodeID();
                pkt.header.length = sizeof(segptr->header) + segptr->header.length;
                pkt.header.type = SIP;
                if (son_sendpkt(next_id, &pkt, son_conn) < 0) {
                    return;  // 不可接受 SON 的异常
                }
                else {
                    log("send pkt successfully");
                }
            }
            else {
                warn("refuse to route unroutable dest %d", dst_id);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    log("SIP layer is starting, pls wait...");

    //初始化全局变量
    nr_nbrs = topology_getNbrNum();
    nct = nbrcosttable_create();
    dv = dvtable_create(nct);
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
        log("can't connect to SON process");
        exit(1);
    }

    //启动线程处理来自SON进程的进入报文
    pthread_t pkt_handler_thread;
    pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

    //启动路由更新线程
    pthread_t routeupdate_thread;
    pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);

    // 启动邻居判活线程
    pthread_t nbr_alive_thread;
    pthread_create(&nbr_alive_thread, NULL, alive_check, NULL);

    log("SIP layer is started...");
    log("waiting for routes to be established");


    sleep(SIP_WAITTIME);
    puts("===========================");
    pthread_mutex_lock(routingtable_mutex);
    routingtable_print(routingtable);
    pthread_mutex_unlock(routingtable_mutex);
    pthread_mutex_lock(dv_mutex);
    dvtable_print(dv);
    pthread_mutex_unlock(dv_mutex);
    puts("===========================");

    //等待来自STCP进程的连接
    log("waiting for connection from STCP process");
    waitSTCP();
}
