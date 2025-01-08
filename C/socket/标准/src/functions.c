#include "include/functions.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// 定义最大数据长度和最大函数数量
#define MAX_DATA_SIZE 4096
#define MAX_FUNCTIONS 100

// 函数注册表
static function_t function_registry[MAX_FUNCTIONS];
static int function_count = 0; // 当前注册的函数数量

// 初始化函数注册表
void init_function_registry() {
    function_count = 0;
    memset(function_registry, 0, sizeof(function_registry));
}

// 注册新函数
int register_function(int id, handler_t handler) {
    if (function_count >= MAX_FUNCTIONS) {
        return -1; // 注册表已满
    }

    // 检查ID是否已存在
    for (int i = 0; i < function_count; i++) {
        if (function_registry[i].id == id) {
            return -2; // ID已存在
        }
    }

    // 添加新函数
    function_registry[function_count].id = id;
    function_registry[function_count].handler = handler;
    function_count++;
    return 0; // 成功
}

// 根据ID获取函数
function_t *get_function_by_id(int id) {
    for (int i = 0; i < function_count; i++) {
        if (function_registry[i].id == id) {
            return &function_registry[i];
        }
    }
    return NULL; // 未找到
}

// 处理函数实现

// 字符串反转
void str_reverse(const char *input, char *output, uint32_t *length) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        output[i] = input[len - 1 - i];
    }
    output[len] = '\0';
    *length = len;
}

// 字符串转大写
void str_upper(const char *input, char *output, uint32_t *length) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        output[i] = toupper(input[i]);
    }
    output[len] = '\0';
    *length = len;
}

// 字符串转小写
void str_lower(const char *input, char *output, uint32_t *length) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        output[i] = tolower(input[i]);
    }
    output[len] = '\0';
    *length = len;
}

// 计算字符串长度
void str_length(const char *input, char *output, uint32_t *length) {
    size_t len = strlen(input);
    snprintf(output, MAX_DATA_SIZE, "%zu", len);  // 确保输出格式正确
    *length = strlen(output);
    output[*length] = '\0'; // 确保 null 终止
}

// 字符串拼接
void str_concat(const char *input, char *output, uint32_t *length) {
    snprintf(output, MAX_DATA_SIZE, "%s_%s", input, input);
    *length = strlen(output);
    output[*length] = '\0'; // 确保 null 终止
}

// 初始化默认函数
void init_default_functions() {
    register_function(1, str_reverse); // ID 1：字符串反转
    register_function(2, str_upper);   // ID 2：字符串转大写
    register_function(3, str_lower);   // ID 3：字符串转小写
    register_function(4, str_length);  // ID 4：计算字符串长度
    register_function(5, str_concat);  // ID 5：字符串拼接
}
