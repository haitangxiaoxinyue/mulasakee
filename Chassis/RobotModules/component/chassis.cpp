/**
 *******************************************************************************
 * @file      :chassis.cpp
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
/* Includes ------------------------------------------------------------------*/

#include "chassis.hpp"
/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
/* Private function definitions ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】实现 chassis.hpp 中声明的底盘运动学。
 *
 * 你要做：
 *   1. 逆解：输入 (vx, vy, w)，输出每个轮的轮速 / 电流指令（麦轮逆运动学矩阵）。
 *   2. 对解算结果做限幅，避免超速。
 *
 * 约束 / 提示：
 *   - 解算结果会经电机组件的 encode() 下发，注意输入量的类型（电流 / 转矩）。
 * ==========================================================================*/
namespace robot {

// 设置目标速度：把外部给的 (vx, vy, w) 存进成员变量
void Chassis::setTargetSpeed(float vx, float vy, float w) {
          vx_ = vx;  vy_ = vy;  w_ = w;
}

// 读取第 idx 号轮的角速度
float Chassis::wheelOmega(int idx) const {
    return wheel_omega_[idx];
}
// 执行逆解：车身速度 (vx_, vy_, w_) → 4 个轮的目标角速度
void Chassis::update() {
    // 几何参数
    const float kR  = 0.07786f;          // 轮半径，单位 m
    const float kAB = 0.15338f + 0.15338f; // a+b = 半轮距+半轴距，单位 m

    // 旋转项：w 引起的轮心线速度，单位 m/s
    // w > 0 → 左前左后前进、右前右后后退 → 车顺时针（往右）转
    const float rot = w_ * kAB;

    // 麦轮逆解（X 型，vy 向左为正，w > 0 时车顺时针转）
    wheel_omega_[0] = ( vx_ + vy_ + rot ) / kR;   // 左前 LF（旋转项取正）
    wheel_omega_[1] = ( vx_ - vy_ - rot ) / kR;   // 2 号（实测：右前）
    wheel_omega_[2] = ( vx_ + vy_ - rot ) / kR;   // 3 号（实测：右后）
    wheel_omega_[3] = ( vx_ - vy_ + rot ) / kR;   // 4 号（实测：左后）

    // 限幅：把 4 个轮角速度夹在 [-kMaxOmega, +kMaxOmega] 之间
    const float kMaxOmega = 40.0f;   // 上限 rad/s（≈380 rpm，留余量，待调）
    for (int i = 0; i < 4; ++i) {
        if (wheel_omega_[i] >  kMaxOmega) wheel_omega_[i] =  kMaxOmega;
        if (wheel_omega_[i] < -kMaxOmega) wheel_omega_[i] = -kMaxOmega;
    }
}

}  // namespace robot



