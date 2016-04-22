#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 报告系统错误，并退出程序
 * @param msg 传递给 perror 的消息字符串
 */
#define sys_panic(msg)      \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define panic(fmt, ...)                            \
    do {                                           \
        fprintf(stderr, fmt "\n", ## __VA_ARGS__); \
        exit(EXIT_FAILURE);                        \
    } while (0)

#define log(fmt, ...) \
    printf("[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#endif // COMMON_H
