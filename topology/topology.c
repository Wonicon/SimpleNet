//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数
//
//创建日期: 2015年

#include "topology.h"
#include <constants.h>
#include <common.h>
#include <string.h>
#include <assert.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define TOPOLOGY_FILE "topology.dat"

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname)
{
    // https://paulschreiber.com/blog/2005/10/28/simple-gethostbyname-example/
    // 获知h_addr_list[i]对应的具体类型
    char buf[128];
    sprintf(buf, "%s.nju.edu.cn", hostname);
    struct addrinfo *info;
    getaddrinfo(buf, NULL, NULL, &info);
    struct in_addr in_addr = ((struct sockaddr_in *)info->ai_addr)->sin_addr;
    freeaddrinfo(info);
    return htonl(in_addr.s_addr) & 0xFF;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr *addr)
{
    return htonl(addr->s_addr) & 0xFF;
}

//返回本机第一个非回环地址的IP地址，本机字节序
in_addr_t topology_getIP()
{
    // Acknowledgement:
    // http://stackoverflow.com/questions/20800319/how-do-i-get-my-ip-address-in-c-on-linux
    const char *localhost = "127.0.0.1";
    struct ifaddrs *list_head, *curr;
    getifaddrs(&list_head);
    curr = list_head;
    while (curr) {
        if (curr->ifa_addr && curr->ifa_addr->sa_family == AF_INET) { // This check makes sense!
            struct sockaddr_in *in_addr = (void *)curr->ifa_addr;
            if (strcmp(localhost, inet_ntoa(in_addr->sin_addr))) {  // Not localhost
                in_addr_t ip = htonl(in_addr->sin_addr.s_addr);
                freeifaddrs(list_head);
                return ip;
            }
        }
        curr = curr->ifa_next;
    }
    freeifaddrs(list_head);
    return 0;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
    int ip = topology_getIP();
    return ip ? (ip & 0xFF) : -1;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
    int this_id = topology_getMyNodeID();
    char buf[128], host_1[32], host_2[32];
    int cost;
    FILE *fp = fopen(TOPOLOGY_FILE, "r");
    int nbr = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s%s%n", host_1, host_2, &cost);
        int id_1 = topology_getNodeIDfromname(host_1);
        int id_2 = topology_getNodeIDfromname(host_2);
        if (id_1 == this_id) {
            nbr++;
        }
        else if (id_2 == this_id) {
            nbr++;
        }
    }
    fclose(fp);
    return nbr;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
    return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.
int* topology_getNodeArray()
{
    return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.
//以 0 结尾.
int *topology_getNbrArray()
{
    int nr_nbr = topology_getNbrNum();
    int *nbrs = calloc((size_t)(nr_nbr + 1), sizeof(*nbrs));
    int *end = nbrs;
    FILE *fp = fopen(TOPOLOGY_FILE, "r");
    char buf[128], host_1[32], host_2[32];
    int cost;
    int this_id = topology_getMyNodeID();

    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s%s%n", host_1, host_2, &cost);
        int id_1 = topology_getNodeIDfromname(host_1);
        int id_2 = topology_getNodeIDfromname(host_2);

        // 判断是否是涉及这个结点的关系
        // 如果是则将对方记录下来
        int id_to_insert = -1;
        if (id_1 == this_id) {
            id_to_insert = id_2;
        }
        else if (id_2 == this_id) {
            id_to_insert = id_1;
        }

        // 插入新结点 ID
        if (id_to_insert != -1) {
            int *curr = nbrs;
            for (; curr != end; curr++) {
                if (*curr == id_to_insert) {
                    break;
                }
            }
            // 待插入结点是新的
            if (curr == end) {
                *end++ = id_to_insert;
            }
        }
    }

    assert((end - nbrs) == nr_nbr);
    *end = 0;

    fclose(fp);
    return nbrs;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价.
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
    char buf[128], host_1[32], host_2[32];
    unsigned cost;
    FILE *fp = fopen(TOPOLOGY_FILE, "r");
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s%s%u", host_1, host_2, &cost);
        int id_1 = topology_getNodeIDfromname(host_1);
        int id_2 = topology_getNodeIDfromname(host_2);
        if ((id_1 == fromNodeID && id_2 == toNodeID) || (id_2 == fromNodeID && id_1 == toNodeID)) {
            return cost;
        }
    }
    fclose(fp);
    return INFINITE_COST;
}
