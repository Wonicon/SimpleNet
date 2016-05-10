
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
    return abs(node - 185) % MAX_ROUTINGTABLE_SLOTS;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(const routingtable_t *routingtable, int destNodeID)
{
    const routingtable_entry_t *ent = routingtable->hash[makehash(destNodeID)];
    while (ent) {
        if (ent->destNodeID == destNodeID) {
            return ent->nextNodeID;
        } else {
            ent = ent->next;
        }
    }
    log("dst %d cannot be routed", destNodeID);
    return -1;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t *routingtable, int destNodeID, int nextNodeID)
{
    routingtable_entry_t **pEntry = &routingtable->hash[makehash(destNodeID)];
    while (*pEntry) {
        if ((*pEntry)->destNodeID == destNodeID) {
            if ((*pEntry)->nextNodeID == nextNodeID) {
                panic("No effects insert: dest %d, next %d", destNodeID, nextNodeID);
            }
            else {
                (*pEntry)->nextNodeID = nextNodeID;
                return;
            }
        }
        pEntry = &(*pEntry)->next;
    }
    *pEntry = calloc(1, sizeof(**pEntry));
    (*pEntry)->destNodeID = nextNodeID;
    (*pEntry)->nextNodeID = nextNodeID;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t *routingtable_create()
{
    routingtable_t *tab = calloc(1, sizeof(*tab));
    int *nbrs = topology_getNbrArray();
    int nr_nbrs = topology_getNbrNum();
    for (int i = 0; i < nr_nbrs; i++) {
        routingtable_setnextnode(tab, nbrs[i], nbrs[i]);
    }
    free(nbrs);
    return tab;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
//作为参数传入的路由表指针会被设置成 NULL
void routingtable_destroy(routingtable_t **routingtable)
{
    routingtable_entry_t **tab = (*routingtable)->hash;
    for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
        routingtable_entry_t *ent = tab[i];
        while (ent) {
            typeof(ent) temp = ent;
            ent = ent->next;
            free(temp);
        }
    }
    free(*routingtable);
    *routingtable = NULL;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t *tab)
{
    for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
        routingtable_entry_t *ent = tab->hash[i];
        while (ent) {
            log("to %d: next hop %d", ent->destNodeID, ent->nextNodeID);
            ent = ent->next;
        }
    }
}
