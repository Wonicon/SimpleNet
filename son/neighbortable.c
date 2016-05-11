//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include <common.h>
#include "neighbortable.h"
#include "../topology/topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t *nt_create()
{
    int this_id = topology_getMyNodeID();
    in_addr_t this_ip = topology_getIP();
    int nr_nbrs = topology_getNbrNum();
    int *nbrs = topology_getNbrArray();

    log("%d has %d neighbors", this_id, nr_nbrs);

    nbr_entry_t *table = calloc((size_t)nbrs, sizeof(*table));
    for (int i = 0; i < nr_nbrs; i++) {
        log("create nbr table entry for %d", nbrs[i]);
        table[i].conn = -1;
        table[i].nodeID = nbrs[i];
        table[i].nodeIP = (this_ip & (~0xFF)) | nbrs[i];
    }

    free(nbrs);
    return table;
}

//这个函数删除一个邻居表. 它关闭所有连接
//动态分配的表在外面销毁，为了让监听线程可以读取套接字
void nt_destroy(nbr_entry_t *table)
{
    int n = topology_getNbrNum();
    for (int i = 0; i < n; i++) {
        log("Disconnect to neighbor ID %d", table[i].nodeID);
        shutdown(table[i].conn, SHUT_RDWR);
        if (table[i].conn != -1) {
            close(table[i].conn);
        }
    }
    return;
}