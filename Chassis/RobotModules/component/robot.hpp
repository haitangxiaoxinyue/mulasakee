/**
 *******************************************************************************
 * @file      :robot.hpp
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
#ifndef CHASSIS_ROBOT_MODULES_ROBOT_HPP_
#define CHASSIS_ROBOT_MODULES_ROBOT_HPP_

/* Includes ------------------------------------------------------------------*/
#include <cstdint>

#include "module_state.hpp"  // LifeState 枚举
/* Exported macro ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported function prototypes ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】声明底盘板顶层状态机类 Robot（namespace robot）。
 *
 * 你要做：
 *   1. 设计 Robot 类，作为本板每个控制周期（1ms）的入口：汇总各模块数据，
 *      生成控制指令并下发给执行部件。
 *   2. 实现「生命状态」三态切换：Dead（上电未就绪 / 离线）→ Resurrection（恢复
 *      自检）→ Working（正常控制），并定义各状态进入 / 退出时的复位动作。
 *   3. 在 Working 态下按手动控制源与工作模式（分离 / 随动 / 小陀螺）生成底盘运动指令。
 *   4. 提供注册各模块指针的接口（Chassis、Imu、4 轮电机、GimbalChassisComm），并在每个周期按正确顺序更新数据。
 *
 * 约束 / 提示：
 *   - 底盘板与云台板之间只能通过 GimbalChassisComm 的
 *     CAN 帧交换信息，不能直接互相调用函数或共享变量。
 *   - 控制周期入口由 Task/component/main_task.cpp 的定时中断调用，本类自己不要开中断、
 *     不要死等延时。
 *   - 急停：遥控器拨到急停位时，必须保证所有电机指令归零（硬性验收项）。
 *   - 状态机的基类 / 迁移机制要自己设计，本骨架不提供（裁判系统 / 超电 / 蜂鸣器等组件本骨架也没有，可忽略）。
 * ==========================================================================*/

/* Includes ------------------------------------------------------------------*/
namespace hello_world {
namespace motor { class Motor; }
namespace pid { class Pid; }
namespace remote_control { class DT7; }
namespace imu { class Imu; }
}  // namespace hello_world

namespace robot {

class Chassis;            // 前向声明（同 namespace，用指针就够了）
class GimbalChassisComm;  // 前向声明（双板通信）

class Robot {
public:
    void registerChassis(Chassis* chassis);
    void registerImu(hello_world::imu::Imu* imu);
    void registerMotorWheel(int idx, hello_world::motor::Motor* motor);
    void registerPidWheel(int idx, hello_world::pid::Pid* pid);
    void registerRemoteControl(hello_world::remote_control::DT7* remote);
    void registerGimbalChassisComm(GimbalChassisComm* comm);
    void update();   // 1ms 周期入口

private:
    /* 内部工具 -------------------------------------------------------------*/
    void outputZero();                    // 4 轮输出 0：所有"不该动"的场合都走这里
    void enterState(LifeState s);         // 切状态：复位 PID 积分 + 归零 + 记时刻
    void publishGimbalFrame(bool ready, float yaw_rate_ff = 0.0f);  // 打包 0x210；yaw_rate_ff = 底盘自转前馈

    Chassis* chassis_ = nullptr;
    hello_world::imu::Imu* imu_ = nullptr;   // IMU：用于 Resurrection 的自检闸门
    hello_world::motor::Motor* motor_wheel_[4] = {nullptr, nullptr, nullptr, nullptr};
    hello_world::pid::Pid* pid_wheel_[4] = {nullptr, nullptr, nullptr, nullptr};
    hello_world::remote_control::DT7* remote_ = nullptr;
    GimbalChassisComm* comm_ = nullptr;

    /* 云台目标角：由摇杆角速度积分得到（不是摇杆直接映射角度） */
    float yaw_target_   = 0.0f;    // 云台 yaw 目标角，rad
    float pitch_target_ = 0.0f;    // 云台 pitch 目标角，rad
    float pitch_home_   = 0.0f;    // 云台上电时的 pitch 实际角，作为限位中心
    bool  gimbal_target_inited_ = false;  // 是否已用云台回传角对齐过初值
    /* 云台相对车头的偏航角 δ，单位 rad；每个周期从 G2C 帧里取 */
    float delta_ = 0.0f;
    /* 当前底盘模式：由左拨杆决定（上=分离 中=跟随 下=小陀螺） */
    ChassisMode mode_ = ChassisMode::Separate;

    /* 生命状态机 -----------------------------------------------------------*/
    LifeState life_state_ = LifeState::Dead;  // 上电一律从 Dead 开始
    uint32_t  state_enter_tick_ = 0;          // 进入当前状态的时刻，单位 ms
    bool mode_armed_ = false;                 // 拨杆回过上档才允许切跟随/小陀螺
    ChassisMode last_mode_ = ChassisMode::Separate;  // 上一周期模式，用于出小陀螺时重新对齐
    float w_last_ = 0.0f;                     // 上一周期的底盘角速度指令（小陀螺软启动用）
    float last_yaw_ = 0.0f;                   // 上周期 IMU yaw，用于差分求角速度
    float yaw_rate_meas_ = 0.0f;              // 底盘实测偏航角速度 rad/s
    float world_ref_ = 0.0f;                  // 小陀螺锁定的云台世界朝向
    bool  world_ref_valid_ = false;           // 是否已经锁过
};

}  // namespace robot

#endif /* CHASSIS_ROBOT_MODULES_ROBOT_HPP_ */
