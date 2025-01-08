#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define ERROR_MSG_SIZE 256  // 错误信息最大长度
#define CHUNK_SIZE 4096     // 每个数据块的大小（4KB）
#define HEARTBEAT_INTERVAL 5 // 心跳间隔时间（秒）
#define MAX_RETRY_ATTEMPTS 3 // 最大重连次数
#define RETRY_INTERVAL 5     // 初始重连间隔时间（秒）

// 连接模式枚举
typedef enum {
    SHORT_CONNECTION,
    LONG_CONNECTION
} connection_mode_t;

// 数据包头部
typedef struct {
    uint32_t length;       // 数据长度
    int id;                // 处理函数ID
    connection_mode_t mode; // 连接模式
    int is_heartbeat;      // 标识是否为心跳消息
} header_t;

// 服务端响应包
typedef struct {
    int status;               // 状态：0 表示成功，1 表示失败
    char error_msg[ERROR_MSG_SIZE]; // 错误信息
    uint32_t length;          // 响应数据长度
    double server_time;       // 服务端处理时间
    char *data;               // 动态分配的响应数据
} response_t;

// 客户端请求参数
typedef struct {
    int id;                   // 处理函数ID
    uint32_t data_len;        // 请求数据长度
    char *data;               // 动态分配的请求数据
    char *response;           // 动态分配的响应数据
    uint32_t response_len;    // 响应数据长度
    double server_time;       // 服务端处理时间
    double client_time;       // 客户端响应时间
    char error_msg[ERROR_MSG_SIZE]; // 错误信息
    int sock;                 // 套接字描述符
    connection_mode_t mode;   // 连接模式
    pthread_t heartbeat_tid;  // 心跳线程ID
    pthread_mutex_t sock_mutex; // 互斥锁保护 sock
} client_request_t;

// 获取当前时间（秒）
static inline double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

#endif // COMMON_H
