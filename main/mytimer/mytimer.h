#pragma once 



#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 定时器回调函数类型
 * 
 * @param count 当前定时器计数值（单位：us）
 */
typedef void (*mytimer_cb_t)(uint64_t count);

/**
 * @brief 初始化并启动定时器
 * 
 * @param period_us  定时周期（微秒）
 * @param cb         定时回调函数（在任务上下文中执行）
 * @return true      成功
 * @return false     失败
 */
bool mytimer_start(uint64_t period_us, mytimer_cb_t cb);

/**
 * @brief 停止并删除定时器
 */
void mytimer_stop(void);

