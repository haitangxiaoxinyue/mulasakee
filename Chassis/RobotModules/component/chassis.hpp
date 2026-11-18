/**
 *******************************************************************************
 * @file      :chassis.hpp
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
#ifndef CHASSIS_ROBOT_MODULES_CHASSIS_HPP_
#define CHASSIS_ROBOT_MODULES_CHASSIS_HPP_

/* Includes ------------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported function prototypes ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】声明底盘运动模块类 Chassis（namespace robot）。
 *
 * 你要做：
 *   1. 设计 Chassis 类，负责把底盘速度指令 (vx, vy, w) 逆解为每个轮的控制量：
 *      麦克纳姆轮运动学逆解（4 个 M3508 轮电机），输出每个轮的轮速。
 *   2. 提供接口让 Robot 下发目标速度、读取解算结果。
 *
 * 约束 / 提示：
 *   - 几何参数（轮半径、轮距、轮心距、轮位坐标）见考核说明文档。
 *   - 逆解公式需自己推导（注意麦轮斜向布置角），网上「舵向轮 / 麦克纳姆轮逆运动学」资料可参考。
 * ==========================================================================*/





namespace robot {

class Chassis {
public:
    // 设置目标速度：vx 前后、vy 左右、w 旋转
    void setTargetSpeed(float vx, float vy, float w);

    // 执行逆解算出 4 个轮的目标角速度
    void update();

    // 读取第 idx 号轮的角速度（idx ∈ [0,4)，单位 rad/s）
    float wheelOmega(int idx) const;

private:
    float vx_ = 0.0f;            // 目标前后速度
    float vy_ = 0.0f;            // 目标左右速度
    float w_  = 0.0f;            // 目标旋转角速度
    float wheel_omega_[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // 4 轮目标角速度
};

}  // namespace robot


#endif /* CHASSIS_ROBOT_MODULES_CHASSIS_HPP_ */
