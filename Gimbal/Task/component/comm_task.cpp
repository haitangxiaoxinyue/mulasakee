/**
 *******************************************************************************
 * @file      :comm_task.cpp
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
#include "comm_task.hpp"
#include "ins_all.hpp"
#include "HW_can.hpp"
#include "gimbal_chassis_comm.hpp"
#include "can.h"
#include "usart.h"
#include "iwdg.h"

/* 遥控器接收缓冲区（18 字节 DBUS，来源 inc/DT7.hpp） */
static uint8_t rc_rx_buf[hello_world::remote_control::kRcRxDataLen];

/* Exported function definitions ---------------------------------------------*/
void CommTaskInit(void) {
  /* 1. CAN 滤波器 + 启动总线 + 激活接收中断
   *    CAN1：yaw 电机(0x12) + 与底盘板的板间通信(0x210)（FIFO0）
   *    CAN2：pitch 电机(0x11) 独占（FIFO1）
   *    ↑ 这是实测接线，和电路图不一致；照图接会导致 pitch 完全没反应。 */
  CanFilter_Init(&hcan1);
  CanFilter_Init(&hcan2);

  HAL_CAN_Start(&hcan1);
  HAL_CAN_Start(&hcan2);

  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* 2. 串口空闲 + DMA 接收
   *    注意：遥控接收机装在底盘板上，本板这一路目前收不到数据，
   *    保留以兼容骨架要求，不影响控制（目标角来自 CAN2 的 0x210 帧）。 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rc_rx_buf,
                               hello_world::remote_control::kRcRxDataLen);

  /* 3. 喂一次狗 */
  HAL_IWDG_Refresh(&hiwdg);
}

void CommTask(void) {
  /* 1. 云台 yaw / pitch 电机：编码 → 用各自的发送 ID 发出
   *    【实测接线】yaw 在 CAN1，pitch 在 CAN2（和电路图不一致，以实测为准）
   *      CAN1：yaw 电机(0x12) + 底盘板间通信(0x210)
   *      CAN2：pitch 电机(0x11) 独占
   *    发错口 = 电机永远收不到指令，会表现为「不上电、掰得动、推摇杆没反应」。 */
  uint8_t data[8] = {0};
  GetMotorYaw()->encode(data);
  CAN_Send_Msg(&hcan1, data, hello_world::motor::DaMiao::GetTxId(2), 8);

  GetMotorPitch()->encode(data);
  CAN_Send_Msg(&hcan2, data, hello_world::motor::DaMiao::GetTxId(1), 8);

  /* 2. 电机使能状态（先算，下面回传帧的 online 要用）
   * DaMiao 的 is_enabled() 不在 Motor 基类里，要先转回派生类才能调。
   * 这个标志同时也代表「电机真的回过话、angle() 是真实值」。 */
  const bool yaw_alive =
      static_cast<hello_world::motor::DaMiao*>(GetMotorYaw())->is_enabled();
  const bool pitch_alive =
      static_cast<hello_world::motor::DaMiao*>(GetMotorPitch())->is_enabled();

  /* 3. 双板通信：把云台实际角度回传给底盘板（帧 ID 0x211）
   *    ⚠️ online 必须是「两台电机都回过话」才置 1。
   *    否则底盘会在电机还没上报时就把 angle()=0 当成开机姿态记下来，
   *    导致底盘记的 0 和云台真实角差一整圈还多 → 力矩一直打满顶死（实测踩过）。 */
  const bool motors_alive = yaw_alive && pitch_alive;

  robot::GimbalChassisComm *comm = GetGimbalChassisComm();
  comm->gimbal_data().online       = motors_alive;
  comm->gimbal_data().mode         = robot::GimbalMode::JointSpace;
  /* ⚠️ 安全点：yaw 关节角是无限累加的，而 0x211 里塞的是 int16 × 1000，
   *    只能表示 ±32.767 rad ≈ 5.2 圈。小陀螺连续自旋约 11 秒（3 rad/s）就会溢出，
   *    底盘算出的 δ 会瞬间跳变 → 平移方向乱走。
   *    这里先折到 [-π, π] 再发：δ 只用 cos/sin，差 2π 结果一样，不影响控制。 */
  comm->gimbal_data().yaw_actual   = hello_world::AngleNormRad(GetMotorYaw()->angle());
  comm->gimbal_data().pitch_actual = GetMotorPitch()->angle();

  uint8_t comm_data[8] = {0};
  comm->encode(comm_data);
  CAN_Send_Msg(&hcan2, comm_data, 0x211u, 8);
  /* 兼容：板间线也可能插在 CAN1 上，两个口都发一份 */
  CAN_Send_Msg(&hcan1, comm_data, 0x211u, 8);

  /* 4. 喂狗 */
  HAL_IWDG_Refresh(&hiwdg);
}

/* Private function definitions ----------------------------------------------*/
/* UART 空闲中断回调（C 链接，覆盖 HAL 弱函数）。本板暂无接收机，仅维持接收。 */
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                                           uint16_t Size) {
  if (huart == &huart3 &&
      Size >= hello_world::remote_control::kRcRxDataLen) {
    GetRemoteControl()->decode(rc_rx_buf);
    GetRemoteOfflineChecker()->update();
  }
  /* 重新启动下一轮接收（漏了它串口只收一次就停） */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rc_rx_buf,
                               hello_world::remote_control::kRcRxDataLen);
}
