//文件名: sip/sip.h
//
//描述: 这个文件定义用于SIP进程的数据结构和函数
//
//创建日期: 2015年

#ifndef NETWORK_H
#define NETWORK_H

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON();

#endif
