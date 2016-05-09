//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现.
//
// 创建日期: 2015年
//

#include "seg.h"
#include "network.h"
#include <string.h>

//
//
//  用于客户端和服务器的SIP API
//  =======================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: sip_sendseg()和sip_recvseg()是由网络层提供的服务, 即SIP提供给STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据,
// 为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符.
// 即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".
// 成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
// 最后使用send()发送表明段结束的两个字符.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

#define SEG_BEGIN "!&"
#define SEG_END   "!#"
#define SEG_BEGIN_LEN (sizeof(SEG_BEGIN) - sizeof(SEG_BEGIN[0]))
#define SEG_END_LEN (sizeof(SEG_END) - sizeof(SEG_END[0]))

/**
 * @brief The lietral string table for seg's type tag.
 *
 * Used for logging.
 */
static const char *seg_type_sym[] = {
#define S(x) #x
#define TOKEN(x) CYAN S(x) NORMAL
#include "seg_type.h"
#undef TOKEN
#undef S
};

/**
 * @brief Get a segment's type string
 * @param seg the segment pointer
 * @return the corresponding string
 *
 * Note that the string may contain ascii escape code,
 * which will reset the embedded ascii escape code.
 */
const char *seg_type_s(seg_t *seg)
{
    return seg_type_sym[seg->header.type];
}


//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
unsigned short checksum(seg_t *seg);
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segptr)
{
    segptr->header.checksum = checksum(segptr);
    sendseg_arg_t pkt;
    pkt.nodeID = dest_nodeID;
    pkt.seg = *segptr;
    if (send(sip_conn, &pkt, sizeof(pkt), 0) > 0) {
        return 1;
    } else {
        return -1;
    }
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int checkchecksum(seg_t *seg);
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segptr)
{
    sendseg_arg_t pkt;
    if (recv(sip_conn, &pkt, sizeof(pkt), 0) <= 0) {
        return -1;
    }

    *src_nodeID = pkt.nodeID;
    *segptr = pkt.seg;

    if (checkchecksum(segptr) == -1) {
        return 2;
    }

    //内部随机损坏段
    if (seglost(segptr) == 1) {
        //丢包
        return 1;
    } else if (!checkchecksum(segptr)) {
        //段损坏(校验和错误)
        return 2;
    } else {
        return 0;
    }
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
    return 0;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
    return 0;
}

int seglost(seg_t *seg)
{
    //return 0;  // TODO Make development easier!
    if ((rand() % 100) < PKT_LOSS_RATE * 100) {
        if ((rand() % 2) == 0) {
            // 50% to discard the segment
            return 1;
        } else if (seg != NULL) {
            // 50% to pollute the segment
            log("A %s segment will be polluted", seg_type_s(seg));
            int len = sizeof(seg->header) + seg->header.length;
            int error_bit = rand() % (len * 8);  // random bit index
            char *temp = (void *)seg;
            temp = temp + error_bit / 8;
            *temp = *temp ^ (1 << (error_bit % 8));  // flip-flop a bit
            return 0;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static unsigned short calc_checksum(seg_t *seg)
{
    unsigned int sum = 0;
    unsigned short *p = (unsigned short *)seg;

    int len = seg->header.length;
    if (len & 0x1) { // Odd length
        seg->data[len] = 0;
        len = len + 1;
    }

    //报文段首部24个字节，为12个short型数据
    for (int i = 0; i < 12; i++) {
        sum += p[i];
    }

    //有数据段
    if (len > 0) {
        for (int i = 12; i < 12 + len / 2; i++) {
            sum += p[i];
        }
    }

    sum = (sum & 0xFFFF) + (sum >> 16);
    sum = (sum & 0xFFFF) + (sum >> 16); //有进位的话再相加

    return (unsigned short)(~sum);
}

/**
 * @brief Calculate the checksum.
 * @param seg The segment to be calculated.
 * @return The checksum.
 *
 * checksum covers header and data.
 * 1. clear seg->header.checksum to 0
 * 2. [add one zero byte to make the data size to 2n]
 * 3. calc, calc, calc (use 1's complement)
 */
unsigned short checksum(seg_t *seg)
{
    if (seg == NULL) {
        return 0;
    }
    seg->header.checksum = 0;
    return calc_checksum(seg);
}

/**
 * @brief Calculate and check the checksum field.
 * @param seg The segment to be checkd.
 * @return 1 if valid, 0 if invalid.
 */
int checkchecksum(seg_t *seg)
{
    //error
    if (seg == NULL) {
        return 0;
    }
    return calc_checksum(seg) == 0;
}
