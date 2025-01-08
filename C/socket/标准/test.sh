#!/bin/bash

# 配置
server=./server
client=./client
sleep_time=1  # 服务端启动等待时间
loop_count=100  # 循环次数

# 启动服务端
start_server() {
    echo "Starting server..."
    $server &
    server_pid=$!
    sleep $sleep_time  # 等待服务端启动
    if ! ps -p $server_pid > /dev/null; then
        echo "Failed to start server."
        exit 1
    fi
    echo "Server started with PID $server_pid."
}

# 停止服务端
stop_server() {
    echo "Stopping server..."
    kill $server_pid
    wait $server_pid 2>/dev/null
    echo "Server stopped."
}

# 运行测试用例
run_tests() {
    local test_cases=(
        "1 hello"  # 测试字符串反转
        "2 hello"  # 测试转大写
        "3 HELLO"  # 测试转小写
        "4 hello"  # 测试计算长度
        "5 hello"  # 测试字符串拼接
        "6 Unknown function ID: 6" # 测试无效ID
    )

    echo "Running tests for $loop_count iterations..."

    # 初始化统计变量
    total_tests=0
    total_failures=0
    error_ids=()  # 用于存储发生错误的测试用例编号

    for ((i = 1; i <= loop_count; i++)); do
        echo "Iteration $i:"
        for test_case in "${test_cases[@]}"; do
            read id input <<< "$test_case"

            # 调用客户端并输出结果
            echo "Test case: ID=$id, Input=$input"
            output=$($client $id "$input")
            echo "$output"
            echo "----------------------------------------"

            # 统计失败次数
            if echo "$output" | grep -q "Error:"; then
                total_failures=$((total_failures + 1))
                error_ids+=("$id")  # 记录发生错误的测试用例编号
            fi
            total_tests=$((total_tests + 1))
        done
    done

    # 计算错误率
    if [ $total_tests -gt 0 ]; then
        error_rate=$(echo "scale=2; $total_failures / $total_tests * 100" | bc)
    else
        error_rate=0
    fi

    echo "Test results:"
    echo "Total tests: $total_tests"
    echo "Total failures: $total_failures"
    echo "Error rate: $error_rate%"

    # 输出发生错误的测试用例编号
    if [ ${#error_ids[@]} -gt 0 ]; then
        echo "Error IDs: ${error_ids[@]}"
    else
        echo "No errors occurred."
    fi
}

# 主函数
main() {
    start_server
    run_tests
    stop_server
}

# 执行主函数
main
