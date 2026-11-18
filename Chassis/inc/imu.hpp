/**
 *******************************************************************************
 * @file      : imu.hpp
 * @brief     : IMU 设备封装（BMI088 + Mahony + 开机零漂）
 * @history   :
 *  Version     Date            Author          Note
 *  V1.0.0      2024-12-01      Jinletian       1. 初版编写完成
 *  V2.0.0      2026-09-18      Hello World     1. 考核骨架：掏空 Imu 封装类
 *  V2.1.0      2026-11-09      htxxy           1. 底盘板实现：补零漂收敛判定与芯片自检
 *******************************************************************************
 * @attention :
 *  1. 由两个已提供的组件组合而成：
 *       - BMI088：芯片级原始读取（加速度 m/s^2 / 角速度 rad/s）
 *       - ahrs::Mahony：姿态解算（输入角速度 + 加速度，输出欧拉角）
 *  2. 上电后先静止采样 1 秒求陀螺零漂，零漂没收敛前 isReady() 返回 false，
 *     上层状态机据此停留在 Resurrection，不得进入 Working。
 *  3. init() 内部有阻塞操作（芯片配置），只能在 MainTaskInit() 里调，禁止放进 1ms 中断。
 *******************************************************************************
 *  Copyright (c) 2025 Hello World Team, Zhejiang University.
 *  All Rights Reserved.
 *******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef HW_COMPONENTS_DEVICES_IMU_IMU_HPP_
#define HW_COMPONENTS_DEVICES_IMU_IMU_HPP_

/* Includes ------------------------------------------------------------------*/
#include <cmath>
#include <cstdint>

#include "BMI088.hpp"
#include "base.hpp"
#include "mahony.hpp"
#include "spi.h"

namespace hello_world {
namespace imu {

/* ============================================================================
 * Imu：把 BMI088 的原始数据变成"能用的姿态角"
 *
 * 用法：
 *   init()    —— 一次性：配芯片 + 配 Mahony（在 MainTaskInit 里调）
 *   update()  —— 每 1ms：读芯片 → 扣零漂 → 解算姿态（在 MainTask 里调）
 *   yaw()/pitch()/roll() —— 取角度（rad，相对开机姿态）
 *   isReady() —— 零漂收敛 + 芯片数据可信才为 true（状态机的闸门）
 * ==========================================================================*/
class Imu {
 public:
  /* 一次性初始化：配 BMI088 + 配 Mahony */
  void init() {
    constexpr float kSampFreq = 1000.0f;  // 1ms 周期 → 1000Hz
    constexpr float kKp = 0.5f;           // Mahony 比例系数（⏳ 待调）
    constexpr float kKi = 0.0f;           // Mahony 积分系数（⏳ 待调）

    BMI088HWConfig hw_config;
    hw_config.hspi = &hspi1;               // 🔒 SPI1，5.25 MHz（<10 MHz 才合规）
    hw_config.acc_cs_port = GPIOA;         // 🔒 加速度计片选 PA4
    hw_config.acc_cs_pin = GPIO_PIN_4;
    hw_config.gyro_cs_port = GPIOB;        // 🔒 陀螺仪片选 PB0
    hw_config.gyro_cs_pin = GPIO_PIN_0;

    static const float kRotMat[9] = {1.0f, 0.0f, 0.0f,
                                     0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f};  // 🔀 芯片装正 = 单位阵
    bmi088_.init(hw_config, kRotMat);

    /* imuInit 会做芯片自检与寄存器配置，返回 0 才代表一切正常。
     * 云台板那份把这个返回值直接丢了 —— 那样即使芯片没接好，程序也会
     * 一路"假装正常"，最后给出一堆 0 姿态。这里必须留住。 */
    init_err_ = static_cast<uint8_t>(bmi088_.imuInit());

    mahony_.init(kSampFreq, kKp, kKi);
  }

  /* 每 1ms 调用一次 */
  void update() {
    constexpr int kDriftSampleNum = 1000;  // 采 1000 次 ≈ 1 秒（⏳ 可调）
    constexpr float kAccTol = 2.0f;        // 加速度模长容差，m/s^2

    /* 芯片自检没通过（没焊、片选错、SPI 不通）就别再读了。
     * SPI 一次超时是 2ms，读一次要传 4 次，最坏能把 1ms 的中断拖成 8ms，
     * 整个控制节拍都会乱。这里直接返回，保证控制链不受影响。 */
    if (init_err_ != 0) {
      return;
    }

    bmi088_.getData(acc_, gyro_, nullptr);

    /* 阶段一：静止采样 —— 求陀螺零漂，顺便判断芯片数据是否可信 */
    if (drift_cnt_ < kDriftSampleNum) {
      for (int i = 0; i < 3; ++i) {
        gyro_drift_[i] += gyro_[i];
      }
      acc_norm_sum_ += sqrtf(acc_[0] * acc_[0] + acc_[1] * acc_[1] + acc_[2] * acc_[2]);
      ++drift_cnt_;

      if (drift_cnt_ == kDriftSampleNum) {
        for (int i = 0; i < 3; ++i) {
          gyro_drift_[i] /= static_cast<float>(kDriftSampleNum);
        }
        const float acc_norm_avg =
            acc_norm_sum_ / static_cast<float>(kDriftSampleNum);
        /* 静止时三轴合成的模长应该 ≈ 重力加速度。
         * 差太多 = 没读到数据 / 片选接错 / 供电异常，此时姿态不可信。 */
        data_ok_ = (fabsf(acc_norm_avg - kGravAcc) < kAccTol);
        drift_done_ = true;
      }
      return;  // 零漂没算完之前不要更新姿态
    }

    /* 阶段二：扣掉零漂 → Mahony 解算 → 取欧拉角 */
    float gyro_corrected[3];
    for (int i = 0; i < 3; ++i) {
      gyro_corrected[i] = gyro_[i] - gyro_drift_[i];
    }
    mahony_.update(acc_, gyro_corrected);
    mahony_.getEulerAngle(euler_);
  }

  /* 姿态角，单位 rad，相对开机姿态 */
  float roll() const { return euler_[0]; }
  float pitch() const { return euler_[1]; }
  float yaw() const { return euler_[2]; }
  /* 角速度（已去掉零漂），单位 rad/s，i: 0=x 1=y 2=z */
  float gyro(int i) const { return (i >= 0 && i < 3) ? gyro_[i] : 0.0f; }

  /* 是否可用：芯片自检通过 + 零漂收敛 + 数据可信 */
  bool isReady() const { return (init_err_ == 0) && drift_done_ && data_ok_; }

  /* 调试用：0=正常，非 0 见 BMI088ErrState */
  uint8_t initErr() const { return init_err_; }
  bool dataOk() const { return data_ok_; }

 private:
  BMI088 bmi088_;      // 芯片驱动
  ahrs::Mahony mahony_;  // 姿态解算

  uint8_t init_err_ = 0xFF;  // 初值给 0xFF：没调 init() 之前一定不是"就绪"
  float gyro_drift_[3] = {0.0f, 0.0f, 0.0f};  // 陀螺零漂
  float acc_[3] = {0.0f, 0.0f, 0.0f};         // 加速度，m/s^2
  float gyro_[3] = {0.0f, 0.0f, 0.0f};        // 角速度，rad/s
  float euler_[3] = {0.0f, 0.0f, 0.0f};       // [roll pitch yaw]，rad
  float acc_norm_sum_ = 0.0f;                 // 零漂期间加速度模长累加
  int drift_cnt_ = 0;                         // 已采样计数
  bool drift_done_ = false;                   // 零漂是否算完
  bool data_ok_ = false;                      // 芯片数据是否可信
};

}  // namespace imu
}  // namespace hello_world

#endif /* HW_COMPONENTS_DEVICES_IMU_IMU_HPP_ */
