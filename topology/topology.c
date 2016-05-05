//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数
//
//创建日期: 2015年

#include "topology.h"
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname)
{
    // https://paulschreiber.com/blog/2005/10/28/simple-gethostbyname-example/
    // 获知h_addr_list[i]对应的具体类型
    struct hostent *host = gethostbyname(hostname);
    struct in_addr *in_addr = (void *)host->h_addr_list[0];
    return htonl(in_addr->s_addr) & 0xFF;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr *addr)
{
    return htonl(addr->s_addr) & 0xFF;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
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
                return htonl(in_addr->sin_addr.s_addr) & 0xFF;
            }
        }
        curr = curr->ifa_next;
    }
    freeifaddrs(list_head);
    return -1;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
    return 0;
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
int* topology_getNbrArray()
{
    return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价.
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
    return 0;
}
