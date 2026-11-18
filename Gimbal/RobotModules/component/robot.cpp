/**
 *******************************************************************************
 * @file      :robot.cpp
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
#include "robot.hpp"
#include "gimbal.hpp"
#include "motor.hpp"
#include "imu.hpp"
#include "gimbal_chassis_comm.hpp"
#include "motor_DaMiao.hpp"

namespace robot {

/* 小陀螺锁向方式开关：
 *   false —— 用底盘发来的目标角（底盘已经用 IMU 实测角速度把自转补偿掉了）。
 *            这是验证过能用的方案，默认走这条。
 *   true  -- 云台用自己的 IMU 再锁一层（之前试过，会抖会卡，先关掉）。
 */
constexpr bool kUseGimbalWorldHold = false;
/* 小陀螺锁世界朝向：底盘已经把"自转"补偿掉了（底盘用 IMU 实测角速度做前馈），
 * 云台这里只做一个小的比例环，修残余漂移。增益不能大，
 * 云台位置环本身有几十毫秒延迟，外环增益一大就会在目标附近来回找。 */
constexpr float kHoldKp   = 0.05f;
/* 死区：误差小于这个角度就不纠，断掉小幅极限环 */
constexpr float kHoldDz   = 0.008f;
/* 陀螺前馈系数：当前关（底盘已经在补，这里再补会叠加）。
 * 如果哪天底盘那条前馈失效，可以改回 1.0 单独顶。 */
constexpr float kHoldFF   = 0.0f;

void Robot::registerGimbal(Gimbal* gimbal) {
    gimbal_ = gimbal;
}
void Robot::registerImu(hello_world::imu::Imu* imu) {
    imu_ = imu;
}
void Robot::registerYawMotor(hello_world::motor::Motor* motor) {
    yaw_motor_ = motor;
}
void Robot::registerPitchMotor(hello_world::motor::Motor* motor) {
    pitch_motor_ = motor;
}
void Robot::registerRemoteControl(hello_world::remote_control::DT7* remote) {
    remote_ = remote;
}
void Robot::registerGimbalChassisComm(GimbalChassisComm* comm) {
    comm_ = comm;
}

void Robot::update() {
    // 0. 空指针防御：没注册通信对象就直接返回，避免 HardFault
    if (comm_ == nullptr || gimbal_ == nullptr || imu_ == nullptr ||
        yaw_motor_ == nullptr || pitch_motor_ == nullptr) {
        return;
    }

    // 1. 从底盘板发来的 CAN 帧里取出：通信是否正常 + 期望的目标角
    bool offline       = comm_->isOffline();
    bool chassis_ready = comm_->chassis_data().online;
    float cmd_yaw      = comm_->chassis_data().yaw_target;
    float cmd_pitch    = comm_->chassis_data().pitch_target;

    // 1. 失效即归零（最外层）：与底盘断联 / 底盘急停或未就绪 → 电机全零，回 Dead
    if (offline || !chassis_ready) {
        yaw_motor_->setInput(0);
        pitch_motor_->setInput(0);
        life_state_ = LifeState::Dead;
        return;
    }

    // 1.5 等两台电机真正回过话再开始控制
    //     is_enabled() 只有在收到电机反馈帧后才会变 1，所以它等价于「angle() 是真实值」。
    //     不等的话，Gimbal::update() 会拿 angle()=0 当开机姿态，和真实角差一整圈还多。
    const bool motors_alive =
        yaw_motor_ != nullptr && pitch_motor_ != nullptr &&
        static_cast<hello_world::motor::DaMiao*>(yaw_motor_)->is_enabled() &&
        static_cast<hello_world::motor::DaMiao*>(pitch_motor_)->is_enabled();
    if (!motors_alive) {
        yaw_motor_->setInput(0);
        pitch_motor_->setInput(0);
        life_state_ = LifeState::Dead;   // 电机一掉线就退回 Dead，重新走流程
        return;
    }

    // 2. 状态机
    switch (life_state_) {
        case LifeState::Dead:
            yaw_motor_->setInput(0);
            pitch_motor_->setInput(0);
            life_state_ = LifeState::Resurrection;
            break;

        case LifeState::Resurrection:
            /* 自检期间（IMU 采零漂约 1 秒）不要零力矩：
             * 达妙零力矩等于完全松开，云台会被重力和线束拉走，
             * 表现为「上电后跳到一个随机角度，跳完就停」。
             * 改成用位置环顶住当前位置：既不乱跑，也不会滑。 */
            gimbal_->setTarget(yaw_motor_->angle(), pitch_motor_->angle());
            gimbal_->update();
            if (imu_->isReady()) {
                life_state_ = LifeState::Working;
            }
            break;

        case LifeState::Working: {
            /* 小陀螺：用云台自己的 IMU 直接锁世界朝向，和底盘转多快无关。
             * 开始自旋时记一个参考角，之后每周期把偏差按比例补进关节目标。
             * 右摇杆的微调通过「底盘目标角的变化量」叠加到参考角上。 */
            if (kUseGimbalWorldHold && comm_->chassis_data().mode == ChassisMode::Spin) {
                const float world = imu_->yaw();
                if (!hold_valid_) {
                    hold_ref_   = world;
                    hold_valid_ = true;
                } else {
                    float dcmd = cmd_yaw - last_cmd_yaw_;
                    while (dcmd >  3.14159265f) dcmd -= 6.2831853f;
                    while (dcmd < -3.14159265f) dcmd += 6.2831853f;
                    if (fabsf(dcmd) < 0.2f) {   // 只吃摇杆的小步进，跳变丢掉
                        hold_ref_ += dcmd;
                    }
                }
                last_cmd_yaw_ = cmd_yaw;
                float err = hold_ref_ - world;
                while (err >  3.14159265f) err -= 6.2831853f;
                while (err < -3.14159265f) err += 6.2831853f;
                if (fabsf(err) < kHoldDz) {
                    err = 0.0f;   // 死区：够准了就不纠，避免来回摆
                }
                /* 前馈：底板以 gyro_z 转动，关节就反向补同样的量；
                 * 比例项只修剩下的残差 */
                const float ff = -kHoldFF * imu_->gyro(2) * 0.001f;
                cmd_yaw = yaw_motor_->angle() + ff + kHoldKp * err;
            } else {
                hold_valid_ = false;
                last_cmd_yaw_ = cmd_yaw;
            }
            gimbal_->setTarget(cmd_yaw, cmd_pitch);
            gimbal_->update();
            break;
        }
    }
}

}  // namespace robot
