#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "include/common.h"
#include "include/functions.h"
#include "include/log.h"
#include "include/network.h"

void *handle_client(void *arg) {
    int conn_fd = *(int *)arg;
    header_t header;
    response_t response;

    // 初始化响应包
    response.status = 0; // 默认状态为成功
    memset(response.error_msg, 0, ERROR_MSG_SIZE);
    response.length = 0;
    response.server_time = 0;
    response.data = NULL;

    // 接收数据包头部
    if (receive_all(conn_fd, &header, sizeof(header_t)) < 0) {
        LOG_ERROR("Failed to receive header");
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 检查是否为心跳消息
    if (header.is_heartbeat) {
        // 处理心跳消息
        response.status = 0;
        response.length = 0;
        send_all(conn_fd, &response, sizeof(response_t));
        close(conn_fd);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 分配数据缓冲区，额外分配一个字节用于 null 终止符
    char *data = (char *)malloc(header.length + 1);
    if (!data) {
        LOG_ERROR("Failed to allocate memory for data");
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 接收数据
    if (receive_all(conn_fd, data, header.length) < 0) {
        LOG_ERROR("Failed to receive data");
        free(data);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 确保数据以 null 结尾
    data[header.length] = '\0';

    // 根据ID调用处理函数
    function_t *func = get_function_by_id(header.id);
    if (!func) {
        LOG_ERROR("Unknown function ID");
        response.status = 1; // 状态为失败
        snprintf(response.error_msg, ERROR_MSG_SIZE, "Unknown function ID: %d", header.id);
        send_all(conn_fd, &response, sizeof(response_t));
        free(data);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 分配响应数据缓冲区
    response.data = (char *)malloc(header.length * 2); // 假设响应数据不会超过输入数据的两倍
    if (!response.data) {
        LOG_ERROR("Failed to allocate memory for response data");
        free(data);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 处理请求
    double start_time = get_current_time();
    func->handler(data, response.data, &response.length);
    response.server_time = get_current_time() - start_time;

    // 发送响应头部
    if (send_all(conn_fd, &response, sizeof(response_t)) < 0) {
        LOG_ERROR("Failed to send response header");
        free(data);
        free(response.data);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 发送响应数据
    if (send_all(conn_fd, response.data, response.length) < 0) {
        LOG_ERROR("Failed to send response data");
        free(data);
        free(response.data);
        free(arg); // 释放动态分配的 conn_fd
        return NULL;
    }

    // 关闭连接
    close(conn_fd);

    // 释放资源
    free(data);
    free(response.data);
    free(arg); // 释放动态分配的 conn_fd
    return NULL;
}

int main() {
    // 初始化日志模块
    log_init("./logs");

    // 初始化函数注册表并添加默认处理函数
    init_function_registry();
    init_default_functions();

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        LOG_ERROR("Failed to create socket");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG_ERROR("Failed to bind socket");
        return 1;
    }

    if (listen(listen_fd, 10) < 0) {
        LOG_ERROR("Failed to listen on socket");
        return 1;
    }

    LOG_INFO("Server is listening on port 8888...");

    while (1) {
        int *conn_fd = (int *)malloc(sizeof(int)); // 动态分配 conn_fd
        if (!conn_fd) {
            LOG_ERROR("Failed to allocate memory for conn_fd");
            continue;
        }

        *conn_fd = accept(listen_fd, NULL, NULL);
        if (*conn_fd < 0) {
            LOG_ERROR("Failed to accept connection");
            free(conn_fd);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, conn_fd)) {
            LOG_ERROR("Failed to create thread");
            close(*conn_fd);
            free(conn_fd);
        } else {
            pthread_detach(thread); // 分离线程，确保资源被正确释放
        }
    }

    close(listen_fd);
    log_cleanup();
    return 0;
}
