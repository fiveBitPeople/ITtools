#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

// 初始化日志模块
void log_init(const char *log_dir);

// 清理日志模块
void log_cleanup();

// 记录错误日志
void log_error(const char *file, int line, const char *format, ...);

// 记录信息日志
void log_info(const char *file, int line, const char *format, ...);

// 宏定义
#define LOG_ERROR(format, ...) log_error(__FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  log_info(__FILE__, __LINE__, format, ##__VA_ARGS__)

#endif // LOG_H
