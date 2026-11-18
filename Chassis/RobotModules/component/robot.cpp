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
#include "chassis.hpp"
#include "imu.hpp"
#include "motor.hpp"
#include "pid.hpp"
#include "DT7.hpp"
#include "ins_comm.hpp"
#include "gimbal_chassis_comm.hpp"
#include <cmath>

#include "tick.hpp"
/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
/* Private function definitions ----------------------------------------------*/

/* ============================================================================
 * 【新人任务】实现 robot.hpp 中声明的状态机。
 *
 * 你要做：
 *   1. 周期入口：按「更新数据 → 状态机迁移 → 生成指令 → 下发」的顺序组织 update()。
 *   2. 三态实现：Dead 时输出全零并等待就绪条件；Resurrection 时做自检与复位（如等待
 *      电机使能、IMU 零漂收敛）；Working 时执行正常控制逻辑。
 *   3. 各工作模式（分离 / 随动 / 小陀螺）的指令生成分支。
 *
 * 约束 / 提示：
 *   - 状态切换时记得复位上一状态残留的指令，避免电机突变。
 *   - 所有下发指令统一经过本类，急停判断放在最外层，确保任何分支都先过急停。
 * ==========================================================================*/


/* 调试用全局数组：暴露给 Ozone 实时看每个轮的目标 / 实际角速度（单位 rad/s）*/
float dbg_wheel_target[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // 目标角速度
float dbg_wheel_actual[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // 实际角速度

/* 遥控器调试变量（探针）：暴露给 Ozone，判断「遥控数据有没有进板子」*/
float dbg_rc_lv = 0.0f;       // 左摇杆竖直：推它应在 [-1,1] 变化
float dbg_rc_rv = 0.0f;       // 右摇杆竖直：推它应在 [-1,1] 变化
float dbg_rc_rh = 0.0f;       // 右摇杆水平
float dbg_rc_lh = 0.0f;       // 左摇杆水平 → 横移
int   dbg_rc_offline = 0;     // 1=离线(100ms 没收到数据)  0=在线
int   dbg_rc_switch = 0;      // 右拨杆：1=上  2=下(急停)  3=中
/* 状态机调试变量：Ozone 里看这三个就够了 */
int   dbg_life_state   = 0;   // 0=Dead  1=Resurrection  2=Working
int   dbg_link_ok      = 0;   // 1=双板通信正常
int   dbg_gimbal_ready = 0;   // 1=云台两轴电机都回过话
int   dbg_mode = 0;           // 底盘模式：0=分离 1=跟随 2=小陀螺
int   dbg_imu_ready = 0;      // 1=IMU 自检通过、零漂收敛、数据可信
float dbg_yaw_actual = 0.0f;  // 云台回传的 yaw 关节角（未减 kDeltaOffset）
float dbg_delta = 0.0f;       // δ = 云台相对车头的夹角（已减 kDeltaOffset、已折到 ±π）
float dbg_yaw_target = 0.0f;  // 本周期发给云台的 yaw 目标角（看它是不是被对齐过）
float dbg_w_cmd = 0.0f;       // 本周期算出来的底盘角速度指令 w（rad/s）


namespace {
// 最大速度系数
const float kVxMax = 2.0f;   // 最大前进速度，单位 m/s
const float kVyMax = 2.0f;   // 最大横移速度，单位 m/s（左摇杆左右）
const float kWMax  = 5.0f;   // 最大旋转角速度，单位 rad/s（回到慢版，够了再往上加）
/* 云台目标角控制参数：
 * 思路：摇杆控制的是「转动速度」，每 1ms 把速度积分进目标角，
 * 这样推摇杆云台是平滑地转，而不是一推就瞬间转 180°。*/
const float kDt             = 0.001f;   // 控制周期 1ms（来源：TIM6 定时中断）
const float kYawRateMax     = 2.0f;     // 右摇杆推满时 yaw 转速，rad/s（约 115°/s，可调）
const float kPitchRateMax   = 1.0f;     // 左摇杆推满时 pitch 转速，rad/s（可调）
/* pitch 软限位：以「云台上电时的实际姿态」为中心，上下各留一段行程。
 * 用相对量是因为电机编码器零点不一定在机械中位，绝对限位猜错会把电机顶死。*/
const float kGimbalPitchUp   = 0.40f;   // 从开机姿态往上 0.40 rad（约 23°）
const float kGimbalPitchDown = 0.087f;  // 往下 5 度（0.087 rad）：比上电姿态再低一点
constexpr float kDeltaOffset = 0.256f;  /* δ 的零位偏移：枪口正对车头时读到的 yaw_actual（2026-11-11 复标）*/
/* 自检保持时长：就绪条件必须持续成立这么久，才允许进入 Working（可调）*/
constexpr uint32_t kSelfCheckMs = 200;
/* 是否要求 IMU 就绪才允许进 Working：
 * 现场若 IMU 没接好（自检/零漂不通过），可临时改 false 让车先能动，排查完再改回来。*/
constexpr bool kRequireImu = true;
/* 遥控摇杆死区：绝对值小于这个数就当成 0。
 * 为什么需要：松手时摇杆回不到精确的 0（有 0.01~0.03 的漂移），
 * 乘上速度系数后车会自己慢慢爬；云台那两路是积分，漂移会累积成「松手还在转」。*/
constexpr float kRcDeadZone = 0.05f;

/* 调试开关：目标角直接跟随云台实际角（云台不会动）。
 * 用来判断「上电乱跑」是底盘发的目标有问题，还是云台自己有问题。排查完改回 false。*/
constexpr bool kGimbalPassthrough = false;

/* 调试开关：旁路掉 δ 旋转，直接用车体系速度。
 * true 时完全不管云台朝向，只验证「轮子和逆解对不对」。排查完改回 false。*/
constexpr bool kDisableDeltaRotation = false;
const float kPi             = 3.14159265f;
const float kTwoPi          = 6.2831853f;

/* 摇杆死区：|v| < kRcDeadZone 一律返回 0，否则原样返回 */
inline float DeadZone(float v) {
    return (fabsf(v) < kRcDeadZone) ? 0.0f : v;
}
/* 三模式参数 */
constexpr float kFollowKp      = -9.0f;  // 跟随增益（负号：δ 为正时车要往左转）
constexpr float kFollowWMax    = 2.0f;   // 跟随最大角速度 rad/s（不用 kWMax，太猛）
constexpr float kFollowMinW    = 0.2f;   // 跟随最小角速度：克服轮子静摩擦，防"差几度推不动"
constexpr float kFollowDeltaDz = 0.02f;  // δ 死区（≈1.1°）：到位后不再纠，防来回抖
constexpr float kSpinWMax      = 3.0f;   // 小陀螺最大自旋角速度，rad/s（约 172°/s）
constexpr float kSpinAccel     = 4.0f;   // 自旋角加速度上限 rad/s^2：软启动，0.6s 到 3 rad/s，避免起步把云台甩一下
constexpr float kSpinDeadZone  = 0.05f;  // 拨轮死区：|拨轮|<0.05 视为 0（防中位漂移）
constexpr float kSpinHoldKp    = 4.0f;

/* 每轮速度微调系数：哪两个轮子因为配件原因转得慢，就把对应位置调大一点（如 1.10）。
 * 顺序：0=左前 1=右前 2=右后 3=左后（和 ins_motor.cpp 的实测一致） */
const float kWheelTrim[4] = {1.0f, 1.0f, 1.0f, 1.0f};


}  // namespace

namespace robot {

void Robot::registerChassis(Chassis* chassis) {
    chassis_ = chassis;
}

void Robot::registerImu(hello_world::imu::Imu* imu) {
    imu_ = imu;
}

void Robot::registerMotorWheel(int idx, hello_world::motor::Motor* motor) {
    if (idx < 0 || idx >= 4) return;
    motor_wheel_[idx] = motor;
}

void Robot::registerPidWheel(int idx, hello_world::pid::Pid* pid) {
    if (idx < 0 || idx >= 4) return;
    pid_wheel_[idx] = pid;
}

void Robot::registerRemoteControl(hello_world::remote_control::DT7* remote) {
    remote_ = remote;
}

void Robot::registerGimbalChassisComm(GimbalChassisComm* comm) {
    comm_ = comm;
}

/* 4 轮输出 0：所有「不该动」的场合统一走这里。
 * 单点收口 —— 将来新增保护条件时只改这一处，不会漏掉某个分支 */
void Robot::outputZero() {
    for (int i = 0; i < 4; ++i) {
        if (motor_wheel_[i] != nullptr) {
            motor_wheel_[i]->setInput(0);
            dbg_wheel_target[i] = 0.0f;
            dbg_wheel_actual[i] = motor_wheel_[i]->vel();
        }
    }
}

/* 切状态：复位上一状态残留的指令，避免切回去的瞬间猛冲 */
void Robot::enterState(LifeState s) {
    life_state_ = s;
    state_enter_tick_ = hello_world::tick::GetTickMs();

    /* 进入 Working 的那一刻重新对齐云台目标角。
     * 之前是在「第一次收到云台帧」时对齐，那一刻云台板可能刚上电、角度还不稳，
     * 会把一个错误的角度当成目标，导致一开机云台就跑到别的地方。*/
    if (s == LifeState::Working) {
        gimbal_target_inited_ = false;
        yaw_target_ = 0.0f;
        pitch_target_ = 0.0f;
    }

    /* 清 PID：integ_ 是累加量，不清的话切回 Working 会带着旧积分满电流冲一下 */
    for (int i = 0; i < 4; ++i) {
        if (pid_wheel_[i] != nullptr) pid_wheel_[i]->reset();
    }
    outputZero();
}

/* 打包要发给云台板的数据（0x210）：
 * ready=true：底盘允许输出 → online=1，并且积分摇杆得到新的云台目标角
 * ready=false：底盘不允许输出 → online=0，云台收到后自己归零（双板互保） */
void Robot::publishGimbalFrame(bool ready, float yaw_rate_ff) {
    if (comm_ == nullptr || remote_ == nullptr) return;

    /* 第一次收到云台回传的实际角时，把目标角对齐到实际角，
     * 避免上电瞬间云台从当前姿势猛甩到 0。*/
    if (!gimbal_target_inited_ && comm_->gimbal_data().online) {
        yaw_target_   = comm_->gimbal_data().yaw_actual;
        pitch_target_ = comm_->gimbal_data().pitch_actual;
        pitch_home_   = comm_->gimbal_data().pitch_actual;  // 记下开机姿态
        gimbal_target_inited_ = true;
    }

    /* 目标角还没和实际角对齐之前，绝不放行。
     * 否则 yaw_target_ 还是初值 0，云台会朝关节 0 位冲过去 ——
     * 表现为上电后不管云台原来在哪，都自己跑到同一个位置。*/
    if (!gimbal_target_inited_) {
        ready = false;
    }

    if (kGimbalPassthrough) {
        /* 调试模式：目标 = 实际，云台误差恒为 0，想动也动不了 */
        yaw_target_   = comm_->gimbal_data().yaw_actual;
        pitch_target_ = comm_->gimbal_data().pitch_actual;
    } else if (ready) {
        /* 摇杆 → 角速度 → 积分成目标角（过死区：松手不飘）
         * yaw 取负号：右摇杆推右 = 云台顺时针（从车顶看往右转）。*/
        yaw_target_   -= DeadZone(remote_->rc_rh()) * kYawRateMax * kDt;
        yaw_target_   += yaw_rate_ff * kDt;   // 底盘自转前馈：让云台钉住世界方向
        pitch_target_ += DeadZone(remote_->rc_rv()) * kPitchRateMax * kDt;

        /* yaw 绕回 [-π, π]，避免无限累加后超出 int16 编码范围 */
        while (yaw_target_ >  kPi) yaw_target_ -= kTwoPi;
        while (yaw_target_ < -kPi) yaw_target_ += kTwoPi;

        /* pitch 软件限位（相对于开机姿态） */
        if (pitch_target_ > pitch_home_ + kGimbalPitchUp)
            pitch_target_ = pitch_home_ + kGimbalPitchUp;
        if (pitch_target_ < pitch_home_ - kGimbalPitchDown)
            pitch_target_ = pitch_home_ - kGimbalPitchDown;
    }

    comm_->chassis_data().online       = ready;
    comm_->chassis_data().mode         = mode_;   // 云台要用它判断是否进入「锁世界朝向」
    comm_->chassis_data().yaw_target   = yaw_target_;
    comm_->chassis_data().pitch_target = pitch_target_;
    dbg_yaw_target = yaw_target_;   // 给 Ozone 看
}

/* ============================================================================
 * 1ms 周期入口。执行顺序固定为：
 *   0 空指针防御 → 1 采集 → 2 急停/离线 → 3 状态迁移 → 4 非 Working 归零
 *   → 5 Working 控制（算 δ → 摇杆 → 旋转 → 逆解 → 轮速 PID）
 * ==========================================================================*/
void Robot::update() {
    // ---------- 0. 空指针防御 ----------
    if (comm_ == nullptr || remote_ == nullptr || chassis_ == nullptr) {
        return;
    }

    // 【调试】先把遥控器状态存进全局变量，方便 Ozone 看（离线时也能看）
    dbg_rc_offline = GetRemoteOfflineChecker()->isOffline() ? 1 : 0;
    dbg_rc_switch  = (int)remote_->rc_r_switch();
    dbg_rc_lv = remote_->rc_lv();
    dbg_rc_rv = remote_->rc_rv();
    dbg_rc_rh = remote_->rc_rh();
    dbg_rc_lh = remote_->rc_lh();

    // ---------- 1. 采集本周期的外部状态 ----------
    const bool rc_online    = !GetRemoteOfflineChecker()->isOffline();  // 遥控在不在线
    const bool estop        = (remote_->rc_r_switch() == hello_world::remote_control::kSwitchStateDown);
    const bool link_ok      = !comm_->isOffline();          // 云台板还活着吗
    const bool gimbal_ready = comm_->gimbal_data().online;  // 云台两台电机都回过话吗
    /* IMU 是否可用：芯片自检通过 + 零漂收敛 + 数据可信。
     * kRequireImu=false 时可跳过（现场应急用），逻辑一行不用改 */
    const bool imu_ready = (!kRequireImu) || (imu_ != nullptr && imu_->isReady());

    dbg_link_ok      = link_ok ? 1 : 0;
    dbg_gimbal_ready = gimbal_ready ? 1 : 0;
    dbg_imu_ready    = imu_ready ? 1 : 0;

    // ---------- 2. 最外层：急停 / 遥控离线 ----------
    // 任何状态、任何模式，先过这一关。5.1 安全红线要求「失效即归零」
    if (estop || !rc_online) {
        if (life_state_ != LifeState::Dead) {
            enterState(LifeState::Dead);
        } else {
            outputZero();
        }
        publishGimbalFrame(false);  // 通知云台：我也不在，你也归零
        return;
    }

    // ---------- 3. 状态机迁移 ----------
    switch (life_state_) {
        case LifeState::Dead:
            /* 等三个条件同时成立：遥控在线（上面已过）+ 双板通 + 云台就绪 */
            if (link_ok && gimbal_ready) {
                enterState(LifeState::Resurrection);
            }
            break;

        case LifeState::Resurrection:
            /* 自检：双板、云台条件要一直成立，并且 IMU 要完成零漂收敛，
             * 再持续 kSelfCheckMs 才放行进入 Working */
            if (!link_ok || !gimbal_ready) {
                enterState(LifeState::Dead);
            } else if (imu_ready &&
                       (hello_world::tick::GetTickMs() - state_enter_tick_) >= kSelfCheckMs) {
                enterState(LifeState::Working);
            }
            break;

        case LifeState::Working:
            /* 任何一条失效 → 立刻退回 Dead，并清掉残留指令 */
            if (!link_ok || !gimbal_ready) {
                enterState(LifeState::Dead);
            }
            break;

        default:
            enterState(LifeState::Dead);
            break;
    }

    dbg_life_state = (int)life_state_;

    // ---------- 4. 非 Working：电机全零，只把状态同步给云台 ----------
    if (life_state_ != LifeState::Working) {
        outputZero();
        publishGimbalFrame(false);
        return;
    }

    // ---------- 5. Working：正常控制 ----------
    // 5.1 算 δ：云台相对车头的偏航角（云台回传的 yaw 关节角 − 零位偏移）
    dbg_yaw_actual = comm_->gimbal_data().yaw_actual;
    delta_ = hello_world::AngleNormRad(comm_->gimbal_data().yaw_actual - kDeltaOffset);
    dbg_delta = delta_;

    // 5.2 模式选择：左拨杆 上=分离 中=跟随 下=小陀螺
    //     安全锁：必须先看到拨杆在上档一次，才允许切到跟随 / 小陀螺，
    //     否则上电时拨杆停在中/下档，车一进 Working 就自己转起来。
    if (remote_->rc_l_switch() == hello_world::remote_control::kSwitchStateUp) {
        mode_armed_ = true;
    }
    if (!mode_armed_) {
        mode_ = ChassisMode::Separate;
    } else {
        switch (remote_->rc_l_switch()) {
        case hello_world::remote_control::kSwitchStateUp:
            mode_ = ChassisMode::Separate;
            break;
        case hello_world::remote_control::kSwitchStateMid:
            mode_ = ChassisMode::Follow;
            break;
        case hello_world::remote_control::kSwitchStateDown:
            mode_ = ChassisMode::Spin;
            break;
        default:
            mode_ = ChassisMode::Separate;
            break;
        }
    }
    dbg_mode = (int)mode_;

    /* 退出小陀螺的那一刻，云台的朝向已经和底盘的目标角对不上了
     * （自旋期间云台是自己在锁世界方向）。重新对齐一次，
     * 否则一切回分离，云台会突然甩回自旋前的角度。 */
    if (mode_ != last_mode_) {
        if (last_mode_ == ChassisMode::Spin) {
            gimbal_target_inited_ = false;
        }
        last_mode_ = mode_;
    }

    // 5.3 平移速度（云台坐标系：x 沿枪管，y 向云台左手）
    //     左摇杆：竖直=前后，水平=左右横移；旋转统一交给拨轮
    const float vx = DeadZone(remote_->rc_lv()) * kVxMax;   // 左摇杆竖直 → 前进后退
    const float vy = -DeadZone(remote_->rc_lh()) * kVyMax;  // 左摇杆水平 → 左右横移（推左往左）
    float wheel_w = remote_->rc_wheel();                    // 拨轮：三种模式共用（过死区）
    if (fabsf(wheel_w) < kSpinDeadZone) wheel_w = 0.0f;

    // 5.4 底盘角速度 w：三种模式各有不同来源
    float w = 0.0f;
    switch (mode_) {
        case ChassisMode::Separate:
            /* 分离：旋转交给拨轮（左摇杆左右留给横移） */
            w = -wheel_w * kSpinWMax;   // 负号：拨轮往右推 = 车往左转，跟手
            break;

        case ChassisMode::Follow: {
            /* 跟随：目标 δ=0（车头对齐云台）
             * 死区内：直接 0，避免到位后左右抖；死区外：P 闭环，
             * 但至少给 kFollowMinW，否则静摩擦会让车停在差几度的位置 */
            if (fabsf(delta_) < kFollowDeltaDz) {
                w = 0.0f;
            } else {
                w = kFollowKp * delta_;
                if (fabsf(w) < kFollowMinW) {
                    w = (delta_ > 0.0f) ? kFollowMinW : -kFollowMinW;
                }
                if (w >  kFollowWMax) w =  kFollowWMax;
                if (w < -kFollowWMax) w = -kFollowWMax;
            }
            break;
        }

        case ChassisMode::Spin: {
            /* 小陀螺：进入该模式就自动匀速自旋，不用拨轮。
             * 保留角加速度上限（软启动 / 软停止），避免起步把云台甩一下。 */
            float w_t = -kSpinWMax;
            const float dw_max = kSpinAccel * kDt;
            if (w_t > w_last_ + dw_max) w_t = w_last_ + dw_max;
            if (w_t < w_last_ - dw_max) w_t = w_last_ - dw_max;
            w = w_t;
            break;
        }
    }

    // 5.5 云台目标 + 打包 0x210
    /* 先算底盘实测偏航角速度（IMU yaw 差分），小陀螺补偿要用。
     * 注意：必须用 yaw 的差分，不能用陀螺 z 轴 —— 板子的安装方向不是 z 轴朝上，
     * 陀螺 z 量到的不是偏航角速度，用它会完全补偿不到（云台就不抱死了）。 */
    if (imu_ != nullptr) {
        const float yaw_now = imu_->yaw();
        float dyaw = yaw_now - last_yaw_;
        while (dyaw >  kPi) dyaw -= kTwoPi;
        while (dyaw < -kPi) dyaw += kTwoPi;
        /* 跳变防护：正常 1ms 内不会转超过 0.5 rad（500 rad/s）。
         * 超过就是 IMU 的 yaw 翻圈或者丢帧，本周期不拿来补偿。*/
        if (fabsf(dyaw) > 0.5f) {
            dyaw = 0.0f;
        }
        /* 一阶低通：1ms 差分噪声很大（0.001rad 抖动就是 1rad/s），必须滤一下 */
        yaw_rate_meas_ = 0.93f * yaw_rate_meas_ + 0.07f * (dyaw / kDt);
        /* 死区：只滤掉极小的噪声，不能设大，设大了正常自旋也会被当成「没转」 */
        if (fabsf(yaw_rate_meas_) < 0.02f) {
            yaw_rate_meas_ = 0.0f;
        }
        if (yaw_rate_meas_ >  20.0f) yaw_rate_meas_ =  20.0f;
        if (yaw_rate_meas_ < -20.0f) yaw_rate_meas_ = -20.0f;
        last_yaw_ = yaw_now;
    }
    dbg_w_cmd = w;
    w_last_ = w;   // 记录本周期实际发出去的转速，供小陀螺软启动用

    /* 云台自转补偿：
     *   跟随 —— 用指令转速 w（转得慢、打滑少，已验证）
     *   小陀螺 —— 用 IMU 实测的偏航角速度，补多少跟着车实际转多少；
     *             云台那边再用自己的 IMU 做一个小比例环修残余漂移，两者配合。 */
    float yaw_rate_ff = 0.0f;
    if (mode_ == ChassisMode::Follow) {
        yaw_rate_ff = w;
    } else if (mode_ == ChassisMode::Spin) {
        yaw_rate_ff = -yaw_rate_meas_;
    }
    publishGimbalFrame(true, yaw_rate_ff);

    // 5.6 云台系 → 车体系：把速度矢量转 δ 角（w 是角速度，不参与旋转）
    float vx_chassis = vx;
    float vy_chassis = 0.0f;
    if (!kDisableDeltaRotation) {
        const float cos_delta = cosf(delta_);
        const float sin_delta = sinf(delta_);
        vx_chassis = vx * cos_delta - vy * sin_delta;
        vy_chassis = vx * sin_delta + vy * cos_delta;
    }

    // 5.7 交给底盘逆解（车体系速度）→ 4 个轮目标角速度
    chassis_->setTargetSpeed(vx_chassis, vy_chassis, w);
    chassis_->update();

    // 5.8 每个轮做速度闭环：PID
    for (int i = 0; i < 4; ++i) {
        const float target = chassis_->wheelOmega(i) * kWheelTrim[i];  // 目标轮角速度 rad/s（含单轮微调）
        const float actual = motor_wheel_[i]->vel();               // 电机实际角速度 rad/s
        const float out    = pid_wheel_[i]->calc(target, actual);  // PID 输出
        motor_wheel_[i]->setInput(out);                            // 下发给电机
        // 调试用：存下来给 Ozone 看（目标 vs 实际）
        dbg_wheel_target[i] = target;
        dbg_wheel_actual[i] = actual;
    }
}

}  // namespace robot
