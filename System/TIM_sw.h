/**
 * @file TIM_sw.h
 * @brief 软件定时器模块接口（基于TIM2_TimeBase）
 * @version 1.0.0
 * @date 2024-01-01
 * @note 基于TIM2_TimeBase的1ms tick，提供多个软件定时器实例
 * @note 支持单次和周期模式，支持回调函数
 * 
 * @section 使用方法
 * 
 * @subsection 初始化
 * 软件定时器模块由System_Init()自动初始化，无需手动调用TIM_SW_Init()。
 * 如果需要在System_Init()之前使用，可以手动初始化：
 * @code
 * TIM_SW_Init();  // 初始化软件定时器模块（通常由System_Init()自动调用）
 * @endcode
 * 
 * @subsection 创建定时器
 * 创建软件定时器：
 * @code
 * // 创建周期定时器（每1秒触发一次）
 * void timer_callback(TIM_SW_Handle_t handle, void *user_data)
 * {
 *     // 定时器到期回调
 *     // handle: 定时器句柄
 *     // user_data: 用户数据指针
 * }
 * 
 * TIM_SW_Handle_t timer1 = TIM_SW_Create(1000, TIM_SW_MODE_PERIODIC, timer_callback, NULL);
 * if (timer1 != TIM_SW_HANDLE_INVALID)
 * {
 *     // 定时器创建成功
 * }
 * @endcode
 * 
 * @subsection 启动和停止
 * 启动和停止定时器：
 * @code
 * TIM_SW_Start(timer1);   // 启动定时器
 * // ...
 * TIM_SW_Stop(timer1);    // 停止定时器
 * TIM_SW_Restart(timer1); // 重启定时器（停止后重新开始计时）
 * @endcode
 * 
 * @subsection 暂停和恢复
 * 暂停和恢复定时器：
 * @code
 * TIM_SW_Pause(timer1);   // 暂停定时器（保持剩余时间）
 * // ...
 * TIM_SW_Resume(timer1);  // 恢复定时器（从暂停点继续）
 * @endcode
 * 
 * @subsection 查询状态
 * 查询定时器状态：
 * @code
 * if (TIM_SW_IsRunning(timer1))
 * {
 *     uint32_t remaining = TIM_SW_GetRemainingTime(timer1);  // 剩余时间（毫秒）
 *     uint32_t elapsed = TIM_SW_GetElapsedTime(timer1);      // 已运行时间（毫秒）
 * }
 * @endcode
 * 
 * @subsection 修改周期
 * 修改定时器周期：
 * @code
 * TIM_SW_SetPeriod(timer1, 2000);  // 修改周期为2秒
 * @endcode
 * 
 * @subsection 删除定时器
 * 删除定时器（释放资源）：
 * @code
 * TIM_SW_Delete(timer1);  // 删除定时器，句柄失效
 * @endcode
 * 
 * @subsection 完整示例
 * 完整使用示例：
 * @code
 * // 定时器回调函数
 * void led_toggle_callback(TIM_SW_Handle_t handle, void *user_data)
 * {
 *     LED_Toggle(0);  // 切换LED状态
 * }
 * 
 * int main(void)
 * {
 *     System_Init();
 *     LED_Init();
 *     
 *     // 创建周期定时器（每500ms触发一次）
 *     TIM_SW_Handle_t led_timer = TIM_SW_Create(500, TIM_SW_MODE_PERIODIC, 
 *                                                led_toggle_callback, NULL);
 *     
 *     // 启动定时器
 *     TIM_SW_Start(led_timer);
 *     
 *     while (1)
 *     {
 *         // TIM_SW_Update()在TIM2中断中自动调用，无需手动调用
 *         // 主循环可以执行其他任务
 *         Delay_ms(100);
 *     }
 * }
 * @endcode
 * 
 * @section 定时器模式
 * 
 * - **TIM_SW_MODE_ONCE**：单次模式，定时器到期后自动停止
 * - **TIM_SW_MODE_PERIODIC**：周期模式，定时器到期后自动重启
 * 
 * @section 注意事项
 * 
 * 1. **最大数量**：最多支持16个软件定时器实例
 * 2. **回调函数**：回调函数在TIM2中断中执行，应尽量简短，避免长时间阻塞
 * 3. **更新函数**：TIM_SW_Update()在TIM2中断中自动调用，用户无需手动调用
 * 4. **频率切换**：基于TIM2_TimeBase，频率切换时自动适配，确保1秒永远是1秒
 * 5. **溢出处理**：自动处理32位整数溢出（约49.7天后溢出）
 * 6. **暂停功能**：暂停时保持剩余时间，恢复后从暂停点继续计时
 * 7. **周期修改**：运行中修改周期时，已运行时间保持不变
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_TIM_SW_ENABLED：软件定时器模块开关（默认启用）
 * - CONFIG_MODULE_BASE_TIMER_ENABLED：TIM2时间基准模块开关（默认启用，软件定时器依赖）
 * 
 * 最大定时器数量在TIM_sw.c中定义：
 * - TIM_SW_MAX_COUNT：最大定时器数量（默认16）
 * 
 * @section 相关模块
 * 
 * - TIM2_TimeBase：时间基准模块（Drivers/timer/TIM2_TimeBase.c/h）
 * - Delay：延时模块（System/delay.c/h）
 */

#ifndef TIM_SW_H
#define TIM_SW_H

#include <stdint.h>
#include <stdbool.h>

/* 软件定时器句柄类型 */
typedef uint8_t TIM_SW_Handle_t;

/* 无效的定时器句柄 */
#define TIM_SW_HANDLE_INVALID    0xFF

/* 定时器模式 */
typedef enum
{
    TIM_SW_MODE_ONCE = 0,    /**< 单次模式：定时器到期后自动停止 */
    TIM_SW_MODE_PERIODIC     /**< 周期模式：定时器到期后自动重启 */
} TIM_SW_Mode_t;

/* 定时器回调函数类型 */
typedef void (*TIM_SW_Callback_t)(TIM_SW_Handle_t handle, void *user_data);

/**
 * @brief 初始化软件定时器模块
 * @note 必须在TIM2_TimeBase初始化之后调用
 * @note 通常由系统初始化框架自动调用
 */
void TIM_SW_Init(void);

/**
 * @brief 创建软件定时器
 * @param[in] period_ms 定时器周期（毫秒），范围：1~4294967295
 * @param[in] mode 定时器模式（单次/周期）
 * @param[in] callback 回调函数（可为NULL，表示不使用回调）
 * @param[in] user_data 用户数据指针（可为NULL）
 * @return 定时器句柄，失败返回TIM_SW_HANDLE_INVALID
 * @note 定时器创建后处于停止状态，需要调用TIM_SW_Start()启动
 */
TIM_SW_Handle_t TIM_SW_Create(uint32_t period_ms, TIM_SW_Mode_t mode, TIM_SW_Callback_t callback, void *user_data);

/**
 * @brief 启动软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效或定时器未创建）
 */
uint8_t TIM_SW_Start(TIM_SW_Handle_t handle);

/**
 * @brief 停止软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效）
 */
uint8_t TIM_SW_Stop(TIM_SW_Handle_t handle);

/**
 * @brief 重启软件定时器（停止后重新开始计时）
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效）
 */
uint8_t TIM_SW_Restart(TIM_SW_Handle_t handle);

/**
 * @brief 删除软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效）
 * @note 删除后句柄失效，不能再使用
 */
uint8_t TIM_SW_Delete(TIM_SW_Handle_t handle);

/**
 * @brief 检查定时器是否正在运行
 * @param[in] handle 定时器句柄
 * @return true=正在运行，false=未运行或句柄无效
 */
bool TIM_SW_IsRunning(TIM_SW_Handle_t handle);

/**
 * @brief 获取定时器剩余时间
 * @param[in] handle 定时器句柄
 * @return 剩余时间（毫秒），0=已到期或句柄无效
 */
uint32_t TIM_SW_GetRemainingTime(TIM_SW_Handle_t handle);

/**
 * @brief 修改定时器周期
 * @param[in] handle 定时器句柄
 * @param[in] period_ms 新的周期（毫秒），范围：1~4294967295
 * @return 0=成功，1=失败（句柄无效或参数错误）
 * @note 运行中的定时器修改周期后，已运行时间保持不变
 * @note 暂停中的定时器修改周期后，剩余时间保持不变
 */
uint8_t TIM_SW_SetPeriod(TIM_SW_Handle_t handle, uint32_t period_ms);

/**
 * @brief 获取定时器已运行时间
 * @param[in] handle 定时器句柄
 * @return 已运行时间（毫秒），0=未运行或句柄无效
 */
uint32_t TIM_SW_GetElapsedTime(TIM_SW_Handle_t handle);

/**
 * @brief 暂停定时器（保持剩余时间）
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效或未运行）
 * @note 暂停后定时器停止计时，但保留剩余时间
 * @note 使用TIM_SW_Resume()恢复后，从暂停点继续计时
 */
uint8_t TIM_SW_Pause(TIM_SW_Handle_t handle);

/**
 * @brief 恢复定时器（从暂停点继续）
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败（句柄无效或未暂停）
 * @note 恢复后从暂停时的剩余时间继续计时
 */
uint8_t TIM_SW_Resume(TIM_SW_Handle_t handle);

/**
 * @brief 更新软件定时器（在TIM2中断中调用）
 * @note 此函数由TIM2_IRQHandler自动调用，用户无需手动调用
 * @note 检查所有定时器，到期时触发回调
 */
void TIM_SW_Update(void);

/**
 * @brief 检查软件定时器模块是否已初始化
 * @return true=已初始化，false=未初始化
 */
bool TIM_SW_IsInitialized(void);

#endif /* TIM_SW_H */

