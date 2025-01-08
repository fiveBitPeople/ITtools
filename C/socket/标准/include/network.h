#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

// 分块发送数据
int send_all(int sock, const void *buffer, uint32_t length);

// 分块接收数据
int receive_all(int sock, void *buffer, uint32_t length);

#endif // NETWORK_H