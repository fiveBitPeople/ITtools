#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <pthread.h>  // 确保包含 pthread.h
#include "include/log.h"

#define MAX_LOG_FILES 5          // 最大日志文件数
#define MAX_LOG_FILE_SIZE (2 * 1024 * 1024) // 每个日志文件最大大小（2MB）
#define LOG_DIR "./logs"         // 日志文件存放目录

static FILE *log_files[MAX_LOG_FILES] = {NULL};
static int current_log = 0;      // 当前日志文件索引
static char log_file_paths[MAX_LOG_FILES][256]; // 日志文件路径数组
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁保护日志文件

// 初始化日志模块
void log_init(const char *log_dir) {
    // 创建日志目录
    if (access(log_dir, F_OK) != 0) {
        if (mkdir(log_dir, 0755) != 0) {
            perror("Failed to create log directory");
            exit(1);
        }
    }

    // 初始化日志文件
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        snprintf(log_file_paths[i], sizeof(log_file_paths[i]), "%s/log%d.log", log_dir, i + 1);
        log_files[i] = fopen(log_file_paths[i], "a");
        if (!log_files[i]) {
            perror("Failed to open log file");
            exit(1);
        }
        chmod(log_file_paths[i], 0644); // 设置日志文件权限为 0644
    }
}

// 清理日志模块
void log_cleanup() {
    pthread_mutex_lock(&log_mutex);
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        if (log_files[i]) {
            fclose(log_files[i]);
        }
    }
    pthread_mutex_unlock(&log_mutex);
}

// 检查日志文件大小并轮转
static void rotate_logs() {
    pthread_mutex_lock(&log_mutex);

    // 获取当前日志文件大小
    fseek(log_files[current_log], 0, SEEK_END);
    long file_size = ftell(log_files[current_log]);
    fseek(log_files[current_log], 0, SEEK_SET);

    // 如果当前日志文件超过最大大小，切换到下一个日志文件
    if (file_size >= MAX_LOG_FILE_SIZE) {
        fclose(log_files[current_log]);

        // 重命名旧日志文件
        char old_log_path[256];
        snprintf(old_log_path, sizeof(old_log_path), "%s/log%d.old", LOG_DIR, current_log + 1);
        rename(log_file_paths[current_log], old_log_path);

        // 创建新日志文件
        log_files[current_log] = fopen(log_file_paths[current_log], "w");
        if (!log_files[current_log]) {
            perror("Failed to rotate log file");
            exit(1);
        }

        current_log = (current_log + 1) % MAX_LOG_FILES;
    }

    pthread_mutex_unlock(&log_mutex);
}

// 记录日志消息
static void log_message(const char *level, const char *file, int line, const char *format, va_list args) {
    // 获取当前时间
    time_t rawtime;
    struct tm *time_info;
    char timestamp[60];
    time(&rawtime);
    time_info = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);

    // 获取进程 ID
    pid_t pid = getpid();

    // 写入日志文件
    rotate_logs(); // 检查日志文件大小并轮转
    pthread_mutex_lock(&log_mutex);
    fprintf(log_files[current_log], "[%s] [%s] [PID: %d] [%s:%d] ", timestamp, level, pid, file, line);
    vfprintf(log_files[current_log], format, args);
    fprintf(log_files[current_log], "\n");
    fflush(log_files[current_log]); // 确保日志立即写入文件
    pthread_mutex_unlock(&log_mutex);
}

void log_error(const char *file, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("ERROR", file, line, format, args);
    va_end(args);
}

void log_info(const char *file, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("INFO", file, line, format, args);
    va_end(args);
}
