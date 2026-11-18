/**
 *******************************************************************************
 * @file      :gimbal.cpp
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
#include "gimbal.hpp"
#include "motor.hpp"
#include "pid.hpp"
#include "tick.hpp"
#include <cmath>

/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
/* Private function definitions ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】实现 gimbal.hpp 中声明的云台位置控制。
 *
 * 你要做：
 *   1. yaw 环：目标角与反馈角在同一角度域做差，再经位置环 PID 输出转矩 / 电流。
 *   2. pitch 环：加软件限位，超出范围时钳位指令，防止顶到机械限位堵转。
 *   3. 角度回绕：yaw 的差值要归一到 [-π, π] 区间，避免 359°→1° 时反向大转。
 *
 * 约束 / 提示：
 *   - 位置环输出是转矩 / 电流，注意与电机 setInput 的输入类型匹配。
 * ==========================================================================*/
namespace robot {

void Gimbal::registerYawMotor(hello_world::motor::Motor* motor) {
    yaw_motor_ = motor;
}

void Gimbal::registerPitchMotor(hello_world::motor::Motor* motor) {
    pitch_motor_ = motor;
}

void Gimbal::registerYawPid(hello_world::pid::Pid* pid) {
    yaw_pid_ = pid;
}

void Gimbal::registerPitchPid(hello_world::pid::Pid* pid) {
    pitch_pid_ = pid;
}

void Gimbal::setTarget(float yaw, float pitch) {
    yaw_target_ = yaw;
    pitch_target_ = pitch;
}
namespace {
constexpr float kPi = 3.14159265f;   // π，数学常数（180° = π 弧度）
constexpr float kTwoPi = 2.0f * kPi;  // 2π = 360°
/* pitch 软限位：以「开机时的实际姿态」为中心，上下各允许走 kPitchSpan。
 * 用相对量而不是绝对角，是因为电机编码器零点不一定在机械中位，
 * 绝对限位一旦猜错就会把电机顶死在机械限位上 → 过载 → 卡顿。
 * 实测机械限位后，可改成真正的绝对范围。 */
constexpr float kPitchUp   = 0.40f;   // 从开机姿态往上 0.40 rad（约 23°），与底盘一致
constexpr float kPitchDown = 0.087f;  // 往下 5 度（0.087 rad），与底盘一致

/* 电机方向符号：如果云台转向与期望相反（推摇杆往左它往右），把对应改成 -1.0f */
constexpr float kYawDirSign   = 1.0f;
constexpr float kPitchDirSign = 1.0f;

/* 目标角变化率限制：每个 1ms 周期最多变 0.005 rad → 相当于目标角最快 5 rad/s（约 286°/s）
 * 作用：哪怕底盘发来一个「瞬间从 0 跳到 180°」的指令，云台也是平滑地转过去，
 *       而不是被满力矩猛甩。这一步是安全兜底，和底盘发什么无关。
 * 留 5 rad/s 的余量：小陀螺底盘自旋 2 rad/s，云台要反向补 2 rad/s 才跟得上，
 * 限到 2 rad/s 会正好卡在边上、跟不上。 */
constexpr float kMaxTargetStep = 0.005f;
}  // namespace

void Gimbal::update() {
    // ⓪ 空指针防御：没注册就跳过，宁可不动也不要 HardFault
    if (yaw_motor_ == nullptr || pitch_motor_ == nullptr ||
        yaw_pid_ == nullptr || pitch_pid_ == nullptr) {
        return;
    }

    // ① 读反馈角（angle 是当前角度，单位 rad）
    float yaw_ang   = yaw_motor_->angle();
    float pitch_ang = pitch_motor_->angle();

    // ② 目标角平滑（安全兜底）：第一次进来先对齐实际角，之后每周期最多走 kMaxTargetStep
    if (!ref_inited_) {
        yaw_ref_     = yaw_ang;
        pitch_ref_   = pitch_ang;
        pitch_home_  = pitch_ang;   // 记住开机姿态，作为 pitch 限位的中心
        ref_inited_  = true;
    }

    // yaw：先把「目标 − 当前平滑值」绕到 [-π, π]（走最短路径），再按最大步长靠近
    float dyaw = yaw_target_ - yaw_ref_;
    while (dyaw >  kPi) dyaw -= kTwoPi;
    while (dyaw < -kPi) dyaw += kTwoPi;
    if (dyaw >  kMaxTargetStep) dyaw =  kMaxTargetStep;
    if (dyaw < -kMaxTargetStep) dyaw = -kMaxTargetStep;
    yaw_ref_ += dyaw;
    while (yaw_ref_ >  kPi) yaw_ref_ -= kTwoPi;   // 保持在 [-π, π] 内
    while (yaw_ref_ < -kPi) yaw_ref_ += kTwoPi;

    // pitch：同样限斜率，再钳在「开机姿态 ± kPitchSpan」内（防堵转）
    float dpitch = pitch_target_ - pitch_ref_;
    if (dpitch >  kMaxTargetStep) dpitch =  kMaxTargetStep;
    if (dpitch < -kMaxTargetStep) dpitch = -kMaxTargetStep;
    pitch_ref_ += dpitch;
    if (pitch_ref_ > pitch_home_ + kPitchUp) pitch_ref_ = pitch_home_ + kPitchUp;
    if (pitch_ref_ < pitch_home_ - kPitchDown) pitch_ref_ = pitch_home_ - kPitchDown;

    // ③ yaw 位置误差：再绕一次，保证 359°→1° 当成 +2° 而不是 -358°
    float diff = yaw_ref_ - yaw_ang;
    while (diff >  kPi) diff -= kTwoPi;
    while (diff < -kPi) diff += kTwoPi;
    float yaw_ref_abs = yaw_ang + diff;

    // ④ PID：误差 → 力矩
    float torq_yaw   = yaw_pid_->calc(yaw_ref_abs, yaw_ang);
    float torq_pitch = pitch_pid_->calc(pitch_ref_, pitch_ang);

    // ⑤ 下发：力矩给电机（DM 电机 setInput = 纯力矩，单位 N·m，限幅 ±7）
    yaw_motor_->setInput(kYawDirSign * torq_yaw);
    pitch_motor_->setInput(kPitchDirSign * torq_pitch);
}

}//namespace结束