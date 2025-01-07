#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

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

int main() {
    int server_socket, client_sockets[MAX_CLIENTS];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set read_fds;
    int max_fd, activity, i, new_socket, sd;
    char buffer[BUFFER_SIZE];

    // 初始化客户端套接字数组
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // 创建套接字
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    // 绑定地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(1234);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        exit(1);
    }

    // 监听
    if (listen(server_socket, 5) == -1) {
        perror("listen");
        close(server_socket);
        exit(1);
    }

    printf("Server is listening on port 1234...\n");

    while (1) {
        // 清空文件描述符集合
        FD_ZERO(&read_fds);

        // 添加服务器套接字到集合
        FD_SET(server_socket, &read_fds);
        max_fd = server_socket;

        // 添加客户端套接字到集合
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &read_fds);
            }
            if (sd > max_fd) {
                max_fd = sd;
            }
        }

        // 使用 select 监听文件描述符
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select");
            continue;
        }

        // 处理新的连接
        if (FD_ISSET(server_socket, &read_fds)) {
            new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket == -1) {
                perror("accept");
                continue;
            }

            // 将新套接字添加到客户端数组
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("New client connected, socket fd: %d\n", new_socket);
                    break;
                }
            }
        }

        // 处理客户端数据
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &read_fds)) {
                int data_length;
                // 读取长度
                if (readn(sd, &data_length, 4) != 4) {
                    printf("Client disconnected, socket fd: %d\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                    continue;
                }
                data_length = ntohl(data_length);

                // 读取数据
                if (readn(sd, buffer, data_length) != data_length) {
                    printf("Client disconnected, socket fd: %d\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                    continue;
                }

                buffer[data_length] = '\0'; // 确保字符串结束
                printf("Received from client %d: %s\n", sd, buffer);

                // 回显数据
                if (writen(sd, &data_length, 4) != 4 || writen(sd, buffer, data_length) != data_length) {
                    printf("Failed to send data to client %d\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                }
            }
        }
    }

    close(server_socket);
    return 0;
}