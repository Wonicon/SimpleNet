//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include "neighbortable.h"
#include "../topology/topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t *nt_create()
{
    int n = topology_getNbrNum();
    int *neighbors = topology_getNbrArray();

    printf("Read %d entries from topology.dat\n", n);

    nbr_entry_t *table = calloc((size_t)n, sizeof(*table));
    for (int i = 0; i < n; i++) {
        // TODO Get hostname or IP address!
        printf("Init neighbor ID %d\n", neighbors[i]);
        table[i].conn = -1;
        table[i].nodeID = neighbors[i];
        table[i].nodeIP = inet_addr("127.0.0.1");
    }

    return table;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t *table)
{
    int n = topology_getNbrNum();
    for (int i = 0; i < n; i++) {
        printf("Disconnect to neighbor ID %d\n", table[i].nodeID);
        close(table[i].conn);
    }
    free(table);
    return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
    return 0;
}
