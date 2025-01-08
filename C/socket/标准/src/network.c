#include <sys/socket.h>
#include <unistd.h>
#include "include/network.h"

// 分块发送数据
int send_all(int sock, const void *buffer, uint32_t length) {
    uint32_t sent = 0;
    const char *ptr = (const char *)buffer;
    while (sent < length) {
        ssize_t send_len = send(sock, ptr + sent, length - sent, 0);
        if (send_len <= 0) {
            return -1; // 发送失败
        }
        sent += send_len;
    }
    return 0; // 发送成功
}

// 分块接收数据
int receive_all(int sock, void *buffer, uint32_t length) {
    uint32_t received = 0;
    char *ptr = (char *)buffer;
    while (received < length) {
        ssize_t recv_len = recv(sock, ptr + received, length - received, 0);
        if (recv_len <= 0) {
            return -1; // 接收失败
        }
        received += recv_len;
    }
    return 0; // 接收成功
}