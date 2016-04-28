//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现.
//
// 创建日期: 2015年
//

#include "seg.h"
#include "network.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

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

unsigned short checksum(seg_t *seg);
int sip_sendseg(int connection, seg_t *segptr)
{
	//caculate checksum
	segptr->header.checksum = checksum(segptr);

    if (send(connection, SEG_BEGIN, SEG_BEGIN_LEN, 0) == -1) {
        return -1;
    }

    if (send(connection, &segptr->header, sizeof(segptr->header), 0) == -1) {
        return -1;
    }

    if (segptr->header.length > 0 && send(connection, segptr->data, segptr->header.length, 0) == -1) {
        return -1;
    }

    if (send(connection, SEG_END, SEG_END_LEN, 0) == -1) {
        return -1;
    }

    return 1;
}

// 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段. 我们建议你使用recv()一次接收一个字节.
// 你需要查找"!&", 然后是seg_t, 最后是"!#". 这实际上需要你实现一个搜索的FSM, 可以考虑使用如下所示的FSM.
// SEGSTART1 -- 起点
// SEGSTART2 -- 接收到'!', 期待'&'
// SEGRECV -- 接收到'&', 开始接收数据
// SEGSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 这里的假设是"!&"和"!#"不会出现在段的数据部分(虽然相当受限, 但实现会简单很多).
// 你应该以字符的方式一次读取一个字节, 将数据部分拷贝到缓冲区中返回给调用者.
//
// 注意: 还有一种处理方式可以允许"!&"和"!#"出现在段首部或段的数据部分. 具体处理方式是首先确保读取到!&，然后
// 直接读取定长的STCP段首部, 不考虑其中的特殊字符, 然后按照首部中的长度读取段数据, 最后确保以!#结尾.
//
// 注意: 在你剖析了一个STCP段之后,  你需要调用seglost()来模拟网络中数据包的丢失.
// 在sip_recvseg()的下面是seglost()的代码.
//
// 如果段丢失了, 就返回1, 否则返回0.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int checkchecksum(seg_t *seg);
int sip_recvseg(int connection, seg_t *segptr)
{
    char indicator;        // 是否开始读取 boundary_markup
    char boundary_markup;  // 标记界限字符的类型

    // 找到 "!&" 起始标记
    do {
        if (!Recv(connection, &indicator, sizeof(indicator))) {
            return -1;
        }
    } while (indicator != '!');
    Recv(connection, &boundary_markup, sizeof(boundary_markup));
    if (boundary_markup != '&') {
        return 0;  // 段损坏
    }

    // 读取段
    Recv(connection, &segptr->header, sizeof(segptr->header));
    Recv(connection, segptr->data, sizeof(*segptr->data) * segptr->header.length);
	if(checkchecksum(segptr) == -1)
		return 0;

    // 检查结束标记 "!#"
    indicator = '\0';
    Recv(connection, &indicator, sizeof(indicator));
    if (indicator != '!') {
        return 0; //段损坏
    }

    Recv(connection, &boundary_markup, sizeof(boundary_markup));
    if (boundary_markup != '#') {
        return 0;
    }

    return seglost();
    //return 0;
}

int seglost(seg_t *seg)
{
    log("Wos, valid");
    if ((rand() % 100) < PKT_LOSS_RATE * 100) {
        if ((rand() % 2) == 0) {
            // 50% to discard the segment
            return 1;
        }
        else if (seg != NULL) {
            // 50% to pollute the segment
            log("A %s segment will be polluted", seg_type_s(seg));
            int len = sizeof(seg->header) + seg->header.length;
            int error_bit = rand() % (len * 8);  // random bit index
            char *temp = (void *)seg;
            temp = temp + error_bit / 8;
            *temp = *temp ^ (1 << (error_bit % 8));  // flip-flop a bit
            return 0;
        }
        else {
            return 0;
        }
    }
    else {
        return 0;
    }
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
	//error
	if(seg == NULL)
		return 0;
	
	seg->header.checksum = 0;
	int len = seg->header.length;
	unsigned int sum = 0;
	unsigned short *p = (unsigned short*)seg;

	//报文段首部24个字节，为12个short型数据
	int i;
	for(i = 0; i < 12; i++)
		sum += p[i];

	//有数据段
	if(len > 0) {
		//如果数据段为奇数长度，在最后填充一个0字节
		if(len % 2 == 1) {
			seg->data[len] = 0;
			len++;
		}

		for(i = 12; i < 12 + len; i++)
			sum += p[i];
	}

	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = (sum & 0xFFFF) + (sum >> 16); //有进位的话再相加

	//log("send checksum = %x", sum);
	return (unsigned short)(~sum);
}

/**
 * @brief Calculate and check the checksum field.
 * @param seg The segment to be checkd.
 * @return 1 if valid, 0 if invalid.
 */
int checkchecksum(seg_t *seg)
{
    //error
	if(seg == NULL)
		return 0;
    
	int len = seg->header.length;
	//log("checksum in receive %x",seg->header.checksum);
	unsigned int sum = 0;
	unsigned short *p = (unsigned short*)seg;

	int i;
	for(i = 0; i < 12; i++)
		sum += p[i];

	if(len > 0) {
		if(len % 2 == 1) {
			seg->data[len] = 0;
			len++;
		}

		for(i = 12; i < 12 + len; i++)
			sum += p[i];
	}

	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = (sum & 0xFFFF) + (sum >> 16);

	//log("receive checksum = %x",sum);
	if((unsigned short)(~sum) != 0)
		return -1;
	else
		return 1;
}
