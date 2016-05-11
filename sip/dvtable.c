#include <common.h>
#include <constants.h>
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//我们只需要维护结点自身的距离矢量，距离矢量更新公式为；
//D(X,Y) = min{ cost(X,V) + D(V,Y) }
//其中cost(X,V)即邻居表的内容，D(V,Y)在每次获得更新报文时获取。
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t *dvtable_create(const nbr_cost_entry_t *nct)
{
    // distance vector table
    dv_t *dvt = calloc(1, sizeof(*dvt));
    dvt->nodeID = topology_getMyNodeID();

    // 初始化成无效项
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        dvt->dvEntry[i].nodeID = -1;
        dvt->dvEntry[i].cost = INFINITE_COST;
    }

    // 用邻居代价表初始化最初的距离矢量
    int n = topology_getNbrNum();
    for (int i = 0; i < n; i++) {
        dvt->dvEntry[i].nodeID = nct[i].nodeID;
        dvt->dvEntry[i].cost = nct[i].cost;
    }

    return dvt;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t **dvtable)
{
    free(*dvtable);
    *dvtable = NULL;
}

//更新当前结点的距离矢量，也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t *dvt, int toNodeID, unsigned int cost)
{
    // 搜索元素
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (dvt->dvEntry[i].nodeID == toNodeID) {
            dvt->dvEntry[i].cost = cost;
            return 1;
        }
    }

    // 插入新的元素
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (dvt->dvEntry[i].nodeID == -1) {
            dvt->dvEntry[i].nodeID = toNodeID;
            dvt->dvEntry[i].cost = cost;
            return 1;
        }
    }

    // miss 且表满
    return -1;
}

//这个函数返回距离矢量的链路代价.
//如果这个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
//具体是谁的距离矢量，调用者应当清楚
unsigned int dvtable_getcost(dv_t *dv, int toNodeID)
{
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (dv->dvEntry[i].nodeID == toNodeID) {
            return dv->dvEntry[i].cost;
        }
    }
    return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t *dv)
{
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        dv_entry_t *ent = &dv->dvEntry[i];
        if (ent->nodeID != -1) {
            printf("distance vector %d -> %d : %d\n",
                   dv->nodeID, ent->nodeID, ent->cost);
        }
    }
}
