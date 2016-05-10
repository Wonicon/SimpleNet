#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

// Terminology according to
// https://en.wikipedia.org/wiki/ANSI_escape_code#Sequence_elements
#define ESC "\033"
#define CSI ESC "["

#define CYAN    CSI"1;36m"
#define GREEN   CSI"0;32m"
#define RED     CSI"1;31m"
#define ORANGE  CSI"1;33m"
#define MAGENTA CSI"1;35m"
#define NORMAL  CSI"0m"

#define STR(x) #x

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
    fprintf(stderr, GREEN "[%s:%d] " NORMAL fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define warn(fmt, ...) \
    fprintf(stderr, RED "[%s:%d] " NORMAL fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define Assert(expr, fmt, ...)                                         \
    do {                                                               \
        if (!(expr)) {                                                 \
            panic("\"" STR(expr) "\"" " failed: " fmt, ## __VA_ARGS__); \
        }                                                              \
    } while (0)

#endif // COMMON_H
