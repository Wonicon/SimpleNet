#include "nbrcosttable.h"
#include "../topology/topology.h"
#include <common.h>
#include <constants.h>


//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t *nbrcosttable_create()
{
    int nr_nbrs = topology_getNbrNum();
    int *nbrs = topology_getNbrArray();
    int this_id = topology_getMyNodeID();

    nbr_cost_entry_t *tab = calloc(nr_nbrs, sizeof(*tab));

    for (int i = 0 ; i < nr_nbrs; i++) {
        tab[i].nodeID = nbrs[i];
        tab[i].cost = topology_getCost(this_id, tab[i].nodeID);
    }

    free(nbrs);
    return tab;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t **nct)
{
    free(*nct);
    *nct = NULL;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(const nbr_cost_entry_t *nct, int nodeID)
{
    int nr_nbrs = topology_getNbrNum();
    for (int i = 0; i < nr_nbrs; i++) {
        if (nct[i].nodeID == nodeID) {
            return nct[i].cost;
        }
    }
    warn("nbr %d not found", nodeID);
    return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t *nct)
{
    int nr_nbrs = topology_getNbrNum();
    for (int i = 0; i < nr_nbrs; i++) {
        printf("cost %d: %d\n", nct[i].nodeID, nct[i].cost);
    }
}
