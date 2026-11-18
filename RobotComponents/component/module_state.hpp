/**
 *******************************************************************************
 * @file      :module_state.hpp
 * @brief     :
 * @history   :
 *  Version     Date            Author          Note
 *  V0.9.0      yyyy-mm-dd      <author>        1. <note>
 *******************************************************************************
 * @attention :
 *******************************************************************************
 *  Copyright (c) 2024 Hello World Team, Zhejiang University.
 *  All Rights Reserved.
 *******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef MODULE_STATE_HPP_
#define MODULE_STATE_HPP_

/* Includes ------------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported function prototypes ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】定义底盘板与云台板共享的状态类型与枚举（namespace robot）。
 *
 * 本文件会被底盘板、云台板同时编译，只能放「纯类型 / 枚举 / 内联函数」，
 * 严禁依赖任何具体硬件（电机 / CAN / HAL），否则另一块板编不过。
 *
 * 你要做：
 *   1. 生命状态枚举：至少区分「上电未就绪 / 恢复自检中 / 正常工作中」三种，
 *      供 RobotModules 的状态机在 Dead / Resurrection / Working 三态间迁移使用。
 *   2. 手动控制源枚举：遥控器 / 键鼠（如有），说明当前控制指令来源。
 *   3. 底盘工作模式枚举：分离 / 随动 / 小陀螺；云台工作模式枚举（关节空间控 yaw、pitch）。
 *   4. 底盘运动状态结构（平移速度 vx、vy + 旋转角速度 w，单位自定并全工程一致）与
 *      云台运动状态结构（yaw、pitch），并给出加减、数乘等常用运算。
 *
 * 约束 / 提示：
 *   - 这些类型会被 RobotModules 状态机、RobotComponents 双板通讯帧、Instance 接线
 *     三处共同使用，字段命名 / 枚举值必须三处一致——动手前先画一张字段对照表。
 *   - 只需覆盖本骨架用到的状态（裁判系统 / 摩擦轮 / 超电相关状态本骨架用不到，可省略）
 * ==========================================================================*/
namespace robot {

/* 生命状态：状态机三态 */
enum class LifeState {
    Dead,          /* 上电未就绪 */
    Resurrection,  /* 恢复自检中 */
    Working,       /* 正常工作中 */
};
/* 控制源：指令来源 */
enum class ControlSource {
    Remote,    /* 遥控器 */
    Keyboard,  /* 键盘 */
};
/* 底盘工作模式 */
enum class ChassisMode {
    Separate,  /* 分离 */
    Follow,    /* 跟随 */
    Spin,      /* 陀螺 */
};
//云台状态
enum class GimbalMode {
    JointSpace,  /* 关节空间控制 yaw、pitch */
};
/* 底盘运动状态：平移速度 + 旋转角速度 */
struct ChassisMotionState {
    float vx = 0.0f;  /* 前进速度，单位 m/s */
    float vy = 0.0f;  /* 横移速度，单位 m/s */
    float w  = 0.0f;  /* 旋转角速度，单位 rad/s */

/* 加法：两个状态对应分量相加 */
ChassisMotionState operator+(const ChassisMotionState& other) const {
    ChassisMotionState r;
    r.vx = vx + other.vx;
    r.vy = vy + other.vy;
    r.w  = w  + other.w;
    return r;
}/* 减法*/
ChassisMotionState operator-(const ChassisMotionState& other) const {
    ChassisMotionState r;
    r.vx = vx - other.vx;
    r.vy = vy - other.vy;
    r.w  = w  - other.w;
    return r;
}
/* 数乘：每个分量都乘 k */
ChassisMotionState operator*(float k) const {
    ChassisMotionState r;
    r.vx = vx * k;
    r.vy = vy * k;
    r.w  = w  * k;
    return r;
}
};
/* 云台运动状态：yaw + pitch */
struct GimbalMotionState {
    float yaw = 0.0f;    /* 偏航角，单位 rad */
    float pitch = 0.0f;  /* 俯仰角，单位 rad */

    /* 加法 */
    GimbalMotionState operator+(const GimbalMotionState& other) const {
        GimbalMotionState r;
        r.yaw = yaw + other.yaw;
        r.pitch = pitch + other.pitch;
        return r;
    }
    /* 减法 */
    GimbalMotionState operator-(const GimbalMotionState& other) const {
        GimbalMotionState r;
        r.yaw = yaw - other.yaw;
        r.pitch = pitch - other.pitch;
        return r;
    }
    /* 数乘 */
    GimbalMotionState operator*(float k) const {
        GimbalMotionState r;
        r.yaw = yaw * k;
        r.pitch = pitch * k;
        return r;
    }
};
}  // namespace robot


#endif /* MODULE_STATE_HPP_ */
