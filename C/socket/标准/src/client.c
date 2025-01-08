#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include "include/common.h"
#include "include/log.h"
#include "include/network.h"

// 重连服务端
static int reconnect_to_server(client_request_t *request) {
    int retry_count = 0;
    int backoff = RETRY_INTERVAL; // 初始重连间隔

    while (retry_count < MAX_RETRY_ATTEMPTS) {
        LOG_INFO("Attempting to reconnect to server (attempt %d/%d)...", retry_count + 1, MAX_RETRY_ATTEMPTS);

        // 关闭旧的 socket（如果存在）
        pthread_mutex_lock(&request->sock_mutex);
        if (request->sock > 0) {
            close(request->sock);
            request->sock = -1;
        }

        // 创建新的 socket
        request->sock = socket(AF_INET, SOCK_STREAM, 0);
        if (request->sock < 0) {
            LOG_ERROR("Failed to create socket");
            pthread_mutex_unlock(&request->sock_mutex);
            retry_count++;
            sleep(backoff);
            backoff = backoff * 2 + (rand() % 1000) / 1000.0; // 指数退避加随机 jitter
            continue;
        }

        // 连接服务端
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        serv_addr.sin_port = htons(SERVER_PORT);

        if (connect(request->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            LOG_ERROR("Failed to connect to server");
            pthread_mutex_unlock(&request->sock_mutex);
            retry_count++;
            sleep(backoff);
            backoff = backoff * 2 + (rand() % 1000) / 1000.0; // 指数退避加随机 jitter
            continue;
        }

        LOG_INFO("Reconnected to server successfully");
        pthread_mutex_unlock(&request->sock_mutex);
        return 0; // 重连成功
    }

    LOG_ERROR("Failed to reconnect to server after %d attempts", MAX_RETRY_ATTEMPTS);
    return -1; // 重连失败
}

// 心跳机制线程
static void *heartbeat_thread(void *arg) {
    client_request_t *request = (client_request_t *)arg;
    const char *heartbeat_msg = "HEARTBEAT";
    const char *expected_response = "ACK";
    int bytes_received;
    int heartbeat_timeout_count = 0; // 心跳超时计数器

    while (1) {
        // 检查是否需要关闭连接
        if (request->mode == SHORT_CONNECTION || request->sock <= 0) {
            break;
        }

        // 组装心跳消息的头部
        header_t header;
        header.length = 0;
        header.id = 0;
        header.mode = request->mode;
        header.is_heartbeat = 1;

        // 发送心跳消息的头部
        pthread_mutex_lock(&request->sock_mutex);
        if (send_all(request->sock, &header, sizeof(header_t)) < 0) {
            LOG_ERROR("Failed to send heartbeat header");
            pthread_mutex_unlock(&request->sock_mutex);
            if (reconnect_to_server(request) < 0) {
                break; // 重连失败，退出心跳线程
            }
            continue;
        }

        // 发送心跳消息内容
        if (send_all(request->sock, heartbeat_msg, strlen(heartbeat_msg)) < 0) {
            LOG_ERROR("Failed to send heartbeat message");
            pthread_mutex_unlock(&request->sock_mutex);
            if (reconnect_to_server(request) < 0) {
                break; // 重连失败，退出心跳线程
            }
            continue;
        }

        // 设置接收超时时间
        struct timeval timeout;
        timeout.tv_sec = HEARTBEAT_INTERVAL + 1;
        timeout.tv_usec = 0;
        if (setsockopt(request->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            LOG_ERROR("Setsockopt failed");
            pthread_mutex_unlock(&request->sock_mutex);
            if (reconnect_to_server(request) < 0) {
                break; // 重连失败，退出心跳线程
            }
            continue;
        }

        // 等待服务端响应
        char response[10];
        bytes_received = recv(request->sock, response, sizeof(response) - 1, 0);
        pthread_mutex_unlock(&request->sock_mutex);
        if (bytes_received <= 0) {
            LOG_ERROR("No response to heartbeat, connection may be broken");
            heartbeat_timeout_count++;
            if (heartbeat_timeout_count > MAX_RETRY_ATTEMPTS) {
                LOG_ERROR("Heartbeat timeout count exceeded, exiting heartbeat thread");
                break; // 心跳超时次数超过限制，退出心跳线程
            }
            if (reconnect_to_server(request) < 0) {
                break; // 重连失败，退出心跳线程
            }
            continue;
        }
        response[bytes_received] = '\0';

        // 检查响应是否正确
        if (strcmp(response, expected_response) != 0) {
            LOG_ERROR("Invalid heartbeat response");
            if (reconnect_to_server(request) < 0) {
                break; // 重连失败，退出心跳线程
            }
            continue;
        }

        // 重置心跳超时计数器
        heartbeat_timeout_count = 0;

        // 休眠心跳间隔时间
        sleep(HEARTBEAT_INTERVAL);
    }

    // 关闭连接
    pthread_mutex_lock(&request->sock_mutex);
    if (request->sock > 0) {
        close(request->sock);
        request->sock = -1;
    }
    pthread_mutex_unlock(&request->sock_mutex);
    request->heartbeat_tid = 0; // 线程结束，设置为无效值
    return NULL;
}

// 客户端请求函数
void client_request(client_request_t *request) {
    double start_time = get_current_time();  // 记录请求开始时间

    // 检查socket fd是否有效
    if (request->sock <= 0) {
        if (reconnect_to_server(request) < 0) {
            return; // 重连失败，直接返回
        }
    }

    // 组装请求包头部
    header_t header;
    header.length = request->data_len;
    header.id = request->id;
    header.mode = request->mode;
    header.is_heartbeat = 0; // 标记为正常请求

    // 发送头部
    pthread_mutex_lock(&request->sock_mutex);
    if (send_all(request->sock, &header, sizeof(header_t)) < 0) {
        LOG_ERROR("Failed to send header");
        pthread_mutex_unlock(&request->sock_mutex);
        if (reconnect_to_server(request) < 0) {
            return; // 重连失败，直接返回
        }
        pthread_mutex_lock(&request->sock_mutex);
    }

    // 发送数据
    if (send_all(request->sock, request->data, request->data_len) < 0) {
        LOG_ERROR("Failed to send data");
        pthread_mutex_unlock(&request->sock_mutex);
        if (reconnect_to_server(request) < 0) {
            return; // 重连失败，直接返回
        }
        pthread_mutex_lock(&request->sock_mutex);
    }

    // 接收响应头部
    response_t resp;
    if (receive_all(request->sock, &resp, sizeof(response_t)) < 0) {
        LOG_ERROR("Failed to receive response header");
        pthread_mutex_unlock(&request->sock_mutex);
        if (reconnect_to_server(request) < 0) {
            return; // 重连失败，直接返回
        }
        pthread_mutex_lock(&request->sock_mutex);
    }

    // 分配响应数据缓冲区，额外分配一个字节用于 null 终止符
    request->response = (char *)malloc(resp.length + 1);
    if (!request->response) {
        LOG_ERROR("Failed to allocate memory for response data");
        pthread_mutex_unlock(&request->sock_mutex);
        close(request->sock);
        request->sock = -1;
        return;
    }

    // 接收响应数据
    if (receive_all(request->sock, request->response, resp.length) < 0) {
        LOG_ERROR("Failed to receive response data");
        free(request->response);
        request->response = NULL;
        pthread_mutex_unlock(&request->sock_mutex);
        if (reconnect_to_server(request) < 0) {
            return; // 重连失败，直接返回
        }
        pthread_mutex_lock(&request->sock_mutex);
    }

    // 确保响应字符串以 null 结尾
    request->response[resp.length] = '\0';

    // 计算客户端响应时间
    request->client_time = get_current_time() - start_time;

    // 检查服务端返回的状态
    if (resp.status == 0) {
        // 请求成功
        request->response_len = resp.length;
        request->server_time = resp.server_time;
        request->error_msg[0] = '\0'; // 清空错误信息
        LOG_INFO("Request succeeded. Response: %s", request->response); // 添加成功日志
    } else {
        // 请求失败
        request->response_len = 0;
        request->server_time = 0;
        strncpy(request->error_msg, resp.error_msg, ERROR_MSG_SIZE); // 返回错误信息
        LOG_ERROR("Request failed. Error: %s", request->error_msg); // 添加失败日志
    }

    // 根据连接模式决定是否启动心跳机制
    if (request->mode == LONG_CONNECTION && request->heartbeat_tid == 0) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, heartbeat_thread, request)) {
            LOG_ERROR("Failed to create heartbeat thread");
            pthread_mutex_unlock(&request->sock_mutex);
            close(request->sock);
            request->sock = -1;
            return;
        }
        request->heartbeat_tid = tid;
        pthread_detach(tid); // 分离线程，确保资源被正确释放
    }

    pthread_mutex_unlock(&request->sock_mutex);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <id> <input>\n", argv[0]);
        return 1;
    }

    // 初始化日志模块
    log_init("./logs");

    // 客户端请求参数初始化
    client_request_t request;
    request.id = atoi(argv[1]); // 从命令行参数获取ID
    request.data_len = strlen(argv[2]); // 从命令行参数获取输入数据
    request.data = strdup(argv[2]); // 动态分配请求数据
    request.response = NULL;
    request.response_len = 0;
    request.server_time = 0;
    request.client_time = 0;
    request.error_msg[0] = '\0';
    request.sock = -1; // 初始化为无效值
    request.mode = SHORT_CONNECTION; // 设置连接模式
    request.heartbeat_tid = 0; // 初始化心跳线程ID
    pthread_mutex_init(&request.sock_mutex, NULL); // 初始化互斥锁

    // 发送请求
    client_request(&request);

    // 处理响应
    if (request.response_len > 0) {
        printf("Received response: %s\n", request.response);
    } else {
        printf("Error: %s\n", request.error_msg);
    }

    // 输出客户端响应时间
    printf("Client time: %f s\n", request.client_time);

    // 释放资源
    free(request.data);
    free(request.response);
    if (request.sock > 0) {
        close(request.sock);
    }
    if (request.heartbeat_tid != 0) {
        pthread_join(request.heartbeat_tid, NULL);
    }
    pthread_mutex_destroy(&request.sock_mutex); // 销毁互斥锁
    log_cleanup();
    return 0;
}
