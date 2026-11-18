/**
 *******************************************************************************
 * @file      :gimbal.hpp
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
#ifndef GIMBAL_ROBOT_MODULES_GIMBAL_HPP_
#define GIMBAL_ROBOT_MODULES_GIMBAL_HPP_

/* Includes ------------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported function prototypes ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】声明云台运动模块类 Gimbal（namespace robot）。
 *
 * 你要做：
 *   1. 设计 Gimbal 类，实现 yaw / pitch 两个关节的关节空间位置控制（位置环）。
 *   2. 读入云台目标角度（来自遥控器 / 键鼠 / 底盘经双板通讯传来的期望值），输出
 *      yaw / pitch 电机的控制量。
 *   3. 处理 yaw 轴 360° 连续旋转的角度回绕，pitch 轴用软件限位防堵转。
 *
 * 约束 / 提示：
 *   - yaw / pitch 均为达妙 DM-J4310 电机，反馈角度 / 指令接口见 inc/motor_DaMiao.hpp。
 *   - 位置环 PID 由你自己实现（PID 类），本模块只负责调用。
 * ==========================================================================*/

 namespace hello_world {
namespace motor {
class Motor;
}  // namespace motor
namespace pid {
class Pid;
}  // namespace pid
}  // namespace hello_world

namespace robot {

class Gimbal {
 public:
   void registerYawMotor(hello_world::motor::Motor* motor);
  void registerPitchMotor(hello_world::motor::Motor* motor);
  void registerYawPid(hello_world::pid::Pid* pid);
  void registerPitchPid(hello_world::pid::Pid* pid);
  void setTarget(float yaw, float pitch);
  void update();

 private:
  hello_world::motor::Motor* yaw_motor_ = nullptr;
  hello_world::motor::Motor* pitch_motor_ = nullptr;
  hello_world::pid::Pid* yaw_pid_ = nullptr;
  hello_world::pid::Pid* pitch_pid_ = nullptr;
  float yaw_target_ = 0.0f;    // 期望目标角（来自底盘）
  float pitch_target_ = 0.0f;
  float yaw_ref_ = 0.0f;       // 内部平滑后的目标角（限斜率后）
  float pitch_ref_ = 0.0f;
  float pitch_home_ = 0.0f;    // 开机时 pitch 的实际角，作为限位中心
  bool  ref_inited_ = false;   // 是否已用实际角对齐过初值
};

}  // namespace robot
#endif /* GIMBAL_ROBOT_MODULES_GIMBAL_HPP_ */
