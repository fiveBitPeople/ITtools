#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>

// 处理函数类型定义
typedef void (*handler_t)(const char *, char *, uint32_t *);

// 处理函数结构体
typedef struct {
    int id;               // 函数ID
    handler_t handler;    // 处理函数指针
} function_t;

// 初始化函数注册表
void init_function_registry();

// 动态添加处理函数
int register_function(int id, handler_t handler);

// 根据ID获取处理函数
function_t *get_function_by_id(int id);

#endif // FUNCTIONS_H