#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1234

ssize_t writen(int sock, const void *buf, size_t count) {
    size_t bytes_sent = 0;
    const char *ptr = (const char *)buf;
    while (bytes_sent < count) {
        ssize_t n = send(sock, ptr + bytes_sent, count - bytes_sent, 0);
        if (n <= 0) {
            return n;
        }
        bytes_sent += n;
    }
    return bytes_sent;
}

ssize_t readn(int sock, void *buf, size_t count) {
    size_t bytes_read = 0;
    char *ptr = (char *)buf;
    while (bytes_read < count) {
        ssize_t n = recv(sock, ptr + bytes_read, count - bytes_read, 0);
        if (n <= 0) {
            return n;
        }
        bytes_read += n;
    }
    return bytes_read;
}

// 封装发送消息的接口
int send_message(const char *message) {
    int client_socket;
    struct sockaddr_in server_addr;

    // 创建套接字
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        return -1;
    }

    // 连接到服务器
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_socket);
        return -1;
    }

    // 发送数据长度
    int data_length = strlen(message);
    int net_length = htonl(data_length);
    if (writen(client_socket, &net_length, 4) != 4) {
        perror("send length");
        close(client_socket);
        return -1;
    }

    // 发送数据
    if (writen(client_socket, message, data_length) != data_length) {
        perror("send data");
        close(client_socket);
        return -1;
    }

    // 接收回显数据
    if (readn(client_socket, &net_length, 4) != 4) {
        perror("recv length");
        close(client_socket);
        return -1;
    }
    data_length = ntohl(net_length);

    char buffer[BUFFER_SIZE];
    if (readn(client_socket, buffer, data_length) != data_length) {
        perror("recv data");
        close(client_socket);
        return -1;
    }

    buffer[data_length] = '\0'; // 确保字符串结束
    printf("Received from server: %s\n", buffer);

    close(client_socket);
    return 0;
}

int main() {
    // 测试发送消息
    send_message("Hello, server!");
    send_message("This is a test message.");
    send_message("Goodbye!");
    return 0;
}