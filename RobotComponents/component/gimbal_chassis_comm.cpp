/**
 *******************************************************************************
 * @file      :gimbal_chassis_comm.cpp
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
#include "gimbal_chassis_comm.hpp"

namespace {
constexpr float kAngleScale = 1000.0f;        // 角度放大倍数（精度 0.001 rad）
constexpr uint32_t kOfflineThresMs = 100;     // 离线阈值 100ms
}  // namespace

/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
robot::GimbalChassisComm::GimbalChassisComm(robot::GimbalChassisComm::CodePart code_part,
                                            uint32_t tx_id, uint32_t rx_id) {
  /* TODO【新人任务】:
   * 保存 code_part / tx_id / rx_id，并把 last_rx_tick_ 初始化为当前 GetTickMs()。
   */
  code_part_ = code_part;
  tx_id_ = tx_id;
  rx_id_ = rx_id;
  last_rx_tick_ = hello_world::tick::GetTickMs();
}

void robot::GimbalChassisComm::encode(uint8_t tx_data[8]) {
  /* TODO【新人任务】:
   * 编码己方帧：把本地 ChassisData / GimbalData 按 hpp 里定的字段布局打包进 8 字节。
   *   - 由 code_part_ 决定打包哪个方向（Chassis → ChassisData，Gimbal → GimbalData）。
   *   - 浮点量按约定比例缩放成整数，避免丢位；收发两端比例必须一致。
   *   - 打包完成后由 Task/component/comm_task.cpp 用 tx_id_ 发送。*/
    for (int i = 0; i < 8; ++i) tx_data[i] = 0;   // 先清空

    if (code_part_ == CodePart::Chassis) {
        // 打包底盘数据（C2G）
        tx_data[0] = chassis_data_.online ? 1u : 0u;
        tx_data[1] = (uint8_t)chassis_data_.mode;
        int16_t yaw = (int16_t)(chassis_data_.yaw_target * kAngleScale);
        int16_t pitch = (int16_t)(chassis_data_.pitch_target * kAngleScale);
        tx_data[2] = (uint8_t)(yaw & 0xFF);
        tx_data[3] = (uint8_t)((yaw >> 8) & 0xFF);
        tx_data[4] = (uint8_t)(pitch & 0xFF);
        tx_data[5] = (uint8_t)((pitch >> 8) & 0xFF);
    } else {
        // 打包云台数据（G2C）
        tx_data[0] = gimbal_data_.online ? 1u : 0u;
        tx_data[1] = (uint8_t)gimbal_data_.mode;
        int16_t yaw = (int16_t)(gimbal_data_.yaw_actual * kAngleScale);
        int16_t pitch = (int16_t)(gimbal_data_.pitch_actual * kAngleScale);
        tx_data[2] = (uint8_t)(yaw & 0xFF);
        tx_data[3] = (uint8_t)((yaw >> 8) & 0xFF);
        tx_data[4] = (uint8_t)(pitch & 0xFF);
        tx_data[5] = (uint8_t)((pitch >> 8) & 0xFF);
    }
}

void robot::GimbalChassisComm::decode(const uint8_t rx_data[8]) {
  /* TODO【新人任务】:
   * 解码对方帧：把 8 字节按同样布局还原成对方语义，更新本地缓存，并刷新 last_rx_tick_。
   *   - 严格与 encode() 对称；任何一端偏移写错都会让对面读到错乱数据。
   *   - 建议先写自测：同一结构 encode 后再 decode 应能还原原值。
   */
  if (code_part_ == CodePart::Chassis) {
        // 收到云台帧（G2C），解出 GimbalData
        gimbal_data_.online = (rx_data[0] != 0);
        gimbal_data_.mode = (GimbalMode)rx_data[1];
        int16_t yaw = (int16_t)(rx_data[2] | (rx_data[3] << 8));
        int16_t pitch = (int16_t)(rx_data[4] | (rx_data[5] << 8));
        gimbal_data_.yaw_actual = yaw / kAngleScale;
        gimbal_data_.pitch_actual = pitch / kAngleScale;
    } else {
        // 收到底盘帧（C2G），解出 ChassisData
        chassis_data_.online = (rx_data[0] != 0);
        chassis_data_.mode = (ChassisMode)rx_data[1];
        int16_t yaw = (int16_t)(rx_data[2] | (rx_data[3] << 8));
        int16_t pitch = (int16_t)(rx_data[4] | (rx_data[5] << 8));
        chassis_data_.yaw_target = yaw / kAngleScale;
        chassis_data_.pitch_target = pitch / kAngleScale;
    }
    last_rx_tick_ = hello_world::tick::GetTickMs();
  (void)rx_data;
}

bool robot::GimbalChassisComm::isOffline() const {
  /* TODO【新人任务】:
   * 用 GetTickMs() 与 last_rx_tick_ 的差值判断对方是否离线（超过阈值即离线）。
   * 阈值可自定义（如 100ms），供状态机 / 看门狗判断对方是否还活着。
   */
  return (hello_world::tick::GetTickMs() - last_rx_tick_) > kOfflineThresMs;
}

/* Private function definitions ----------------------------------------------*/
