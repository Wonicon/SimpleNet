//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现.
//
// 创建日期: 2015年
//

#include "seg.h"
#include "network.h"
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

int sip_sendseg(int connection, seg_t *segptr)
{
    if (send(connection, SEG_BEGIN, sizeof(SEG_BEGIN), 0) < sizeof(SEG_BEGIN)) {
        return -1;
    }

    if (send(connection, &segptr->header, sizeof(segptr->header), 0) < sizeof(segptr->header)) {
        return -1;
    }

    if (send(connection, segptr->data, segptr->header.length, 0) < segptr->header.length) {
        return -1;
    }

    if (send(connection, SEG_END, sizeof(SEG_END), 0) < sizeof(SEG_END)) {
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
int sip_recvseg(int connection, seg_t *segptr)
{
    char indicator;        // 是否开始读取 boundary_markup
    char boundary_markup;  // 标记界限字符的类型

    // 找到 "!&" 起始标记
    do {
        Recv(connection, &indicator, sizeof(indicator));
    } while (indicator != '!');
    Recv(connection, &boundary_markup, sizeof(boundary_markup));
    if (boundary_markup != '&') {
        return 0;  // 段损坏
    }

    // 读取段
    Recv(connection, &segptr->header, sizeof(segptr->header));
    Recv(connection, segptr->data, sizeof(*segptr->data) * segptr->header.length);

    // 检查结束标记 "!#"
    indicator = '\0';
    Recv(connection, &indicator, sizeof(indicator));
    if (indicator != '!') {
        return 0;
    }

    Recv(connection, &boundary_markup, sizeof(boundary_markup));
    if (boundary_markup != '#') {
        return 0;
    }

    //return seglost();
    return 1;
}

int seglost()
{
    int random = rand() % 100;
    if(random < PKT_LOSS_RATE * 100) {
        return 1;
    }
    else {
        return 0;
    }
}
