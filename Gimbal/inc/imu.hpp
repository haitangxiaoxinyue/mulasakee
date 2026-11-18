/**
 *******************************************************************************
 * @file      : imu.hpp
 * @brief     : IMU 设备封装（新人填写）
 * @history   :
 *  Version     Date            Author          Note
 *  V1.0.0      2024-12-01      Jinletian       1. 初版编写完成
 *  V2.0.0      2026-09-18      Hello World     1. 考核骨架：掏空 Imu 封装类，
 *                                                只保留 BMI088 芯片驱动 + Mahony 姿态
 *******************************************************************************
 * @attention :
 *  本文件已掏空。新人需基于以下两个已提供的组件自行实现 Imu 封装类：
 *    - BMI088：芯片级原始读取（加速度 m/s² / 角速度 rad/s / 温度）
 *    - ahrs::Mahony：姿态解算（输入角速度 + 加速度，输出四元数/姿态角）
 *  需要新人实现：
 *    - 开机零漂计算（静止采样求角速度均值）
 *    - 姿态角输出 roll()/pitch()/yaw()
 *******************************************************************************
 *  Copyright (c) 2025 Hello World Team, Zhejiang University.
 *  All Rights Reserved.
 *******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef HW_COMPONENTS_DEVICES_IMU_IMU_HPP_
#define HW_COMPONENTS_DEVICES_IMU_IMU_HPP_

/* Includes ------------------------------------------------------------------*/
#include "BMI088.hpp"
#include "mahony.hpp"
#include "spi.h"

/* ============================================================================
 * 【新人任务】在本文件编写 Imu 封装类（namespace hello_world::imu）。
 *
 * 你要做：
 *   1. 组合已提供的 BMI088（芯片原始读取，加速度 m/s² / 角速度 rad/s，见 BMI088.hpp）
 *      与 Mahony（姿态解算 update(acc, gyro) → getEulerAngle，见 mahony.hpp / ahrs.hpp）。
 *   2. 实现开机零漂：静止采样一段时间的角速度均值，后续每帧扣除该零漂再送入 Mahony。
 *   3. 对外提供 roll() / pitch() / yaw()（单位 rad），以及标定是否完成的查询。
 *
 * 约束 / 提示：
 *   - Mahony 的加速度输入需含重力加速度项，单位与坐标系（x 前 y 左 z 上）见 ahrs_base.hpp。
 *   - 零漂未收敛前，底盘 / 云台状态机应保持在 Dead / Resurrection，不得进入 Working。
 *   - 本骨架只提供了 imu.hpp（无 imu.cpp）；若需 .cpp 实现文件，可自行在 src/ 下新增
 *     imu.cpp（CMake 已递归 glob *.c*，无需改配置）。
 * ==========================================================================*/

namespace hello_world {
namespace imu {

class Imu {
 public:
  void init() {
    constexpr float kSampFreq = 1000.0f;  // 1ms 周期 → 1000Hz
    constexpr float kKp = 0.5f;           // Mahony 比例系数（待调）
    constexpr float kKi = 0.0f;           // Mahony 积分系数（待调）

    BMI088HWConfig hw_config;
    hw_config.hspi = &hspi1;
    hw_config.acc_cs_port = GPIOA;
    hw_config.acc_cs_pin = GPIO_PIN_4;
    hw_config.gyro_cs_port = GPIOB;
    hw_config.gyro_cs_pin = GPIO_PIN_0;

    float rot_mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    bmi088_.init(hw_config, rot_mat);
    bmi088_.imuInit();

    mahony_.init(kSampFreq, kKp, kKi);
  }

  void update() {
    constexpr int kDriftSampleNum = 1000;  // 采 1000 次 ≈ 1 秒（自定义）

    bmi088_.getData(acc_, gyro_, nullptr);

    if (drift_sample_count_ < kDriftSampleNum) {
      for (int i = 0; i < 3; ++i) {
        gyro_drift_[i] += gyro_[i];
      }
      drift_sample_count_++;
      if (drift_sample_count_ == kDriftSampleNum) {
        for (int i = 0; i < 3; ++i) {
          gyro_drift_[i] /= kDriftSampleNum;
        }
        ready_ = true;
      }
      return;
    }

    for (int i = 0; i < 3; ++i) {
      gyro_[i] -= gyro_drift_[i];
    }
    mahony_.update(acc_, gyro_);
    mahony_.getEulerAngle(euler_);
  }

  float roll() const { return euler_[0]; }
  float pitch() const { return euler_[1]; }
  float yaw() const { return euler_[2]; }
  /* 角速度（已去掉零漂），单位 rad/s，i: 0=x 1=y 2=z */
  float gyro(int i) const { return (i >= 0 && i < 3) ? gyro_[i] : 0.0f; }
  bool isReady() const { return ready_; }

 private:
  BMI088 bmi088_;
  ahrs::Mahony mahony_;
  float gyro_drift_[3] = {0.0f, 0.0f, 0.0f};
  float acc_[3] = {0.0f, 0.0f, 0.0f};
  float gyro_[3] = {0.0f, 0.0f, 0.0f};
  float euler_[3] = {0.0f, 0.0f, 0.0f};
  int drift_sample_count_ = 0;
  bool ready_ = false;
};

}  // namespace imu
}  // namespace hello_world

#endif /* HW_COMPONENTS_DEVICES_IMU_IMU_HPP_ */
