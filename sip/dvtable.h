//文件名: sip/dvtable.h
//
//描述: 这个文件定义用于距离矢量表的数据结构和函数. 
//
//创建日期: 2015年

#ifndef DVTABLE_H
#define DVTABLE_H

#include "nbrcosttable.h"

//dv_entry_t结构定义
typedef struct distancevectorentry {
	int nodeID;		    //目标节点ID，-1 表示无效项
	unsigned int cost;	//到目标节点的代价
} dv_entry_t;

//一个距离矢量表包含(n+1)个dv_t条目, 其中n是这个节点的邻居数, 剩下的一个是这个节点自身. 
typedef struct distancevector {
	int nodeID;		                  //源节点ID
	dv_entry_t dvEntry[MAX_NODE_NUM]; //一个包含N个dv_entry_t的数组, 其中每个成员包含目标节点ID和从该源节点到该目标节点的代价. N是重叠网络中总的节点数.
} dv_t;

dv_t* dvtable_create(const nbr_cost_entry_t *nct);

void dvtable_destroy(dv_t **dvtable);

int dvtable_setcost(dv_t* dvtable, int toNodeID, unsigned int cost);

unsigned int dvtable_getcost(dv_t* dvtable, int toNodeID);

void dvtable_print(dv_t* dvtable);

#endif
