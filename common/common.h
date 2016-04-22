#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define GREEN   "\033[0;32m"
#define RED     "\033[1;31m"
#define ORANGE  "\033[1;33m"
#define MAGENTA "\033[1;35m"
#define NORMAL  "\033[0m"

/**
 * @brief 报告系统错误，并退出程序
 * @param msg 传递给 perror 的消息字符串
 */
#define sys_panic(msg)          \
    do {                        \
        perror(RED msg NORMAL); \
        exit(EXIT_FAILURE);     \
    } while (0)

#define panic(fmt, ...)                                       \
    do {                                                      \
        fprintf(stderr, RED fmt NORMAL "\n", ## __VA_ARGS__); \
        exit(EXIT_FAILURE);                                   \
    } while (0)

#define log(fmt, ...) \
    printf(GREEN "[%s:%d] " NORMAL fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#endif // COMMON_H
