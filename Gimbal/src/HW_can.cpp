/**
 *******************************************************************************
 * @file      : HW_can.cpp
 * @brief     : CAN 收发基础封装实现
 * @history   :
 *  Version     Date            Author          Note
 *  V1.0.0      2026-09-18      Hello World     1. 考核骨架：CanFilter_Init + CAN_Send_Msg
 *                                                全量提供，接收分发回调掏空
 *******************************************************************************
 * @attention :
 *  接收中断回调 HAL_CAN_RxFifo0/1MsgPendingCallback 已在此预留，仅完成「取 8 字节 +
 *  再次使能接收中断」，具体「按帧 ID 分发」逻辑留给新人：
 *    - M3508 / DM 电机反馈：调用对应电机的 decode(can?_rx_data)
 *    - 双板通讯帧：调用新人自写的协议解码
 *******************************************************************************
 *  Copyright (c) 2024 Hello World Team, Zhejiang University.
 *  All Rights Reserved.
 *******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "HW_can.hpp"
#include "ins_all.hpp"
#include "gimbal_chassis_comm.hpp"
#include "can.h"

/* Private variables ---------------------------------------------------------*/
static CAN_RxHeaderTypeDef rx_header1;
static CAN_RxHeaderTypeDef rx_header2;
static uint8_t can1_rx_data[8];
static uint8_t can2_rx_data[8];
static uint32_t p_tx_mailbox;

/* Exported function definitions ---------------------------------------------*/

void CanFilter_Init(CAN_HandleTypeDef *hcan)
{
  CAN_FilterTypeDef can_filter = {0};

  /* 全收：掩码全 0，任何 ID 均通过 */
  can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
  can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
  can_filter.FilterIdHigh = 0x0000;
  can_filter.FilterIdLow = 0x0000;
  can_filter.FilterMaskIdHigh = 0x0000;
  can_filter.FilterMaskIdLow = 0x0000;
  can_filter.FilterActivation = ENABLE;
  /* CAN1/CAN2 共用滤波器：CAN2 从第 14 组开始，必须先声明分界点，
   * 否则 CAN2 的滤波器组号分配不到，CAN2 收不到任何帧 */
  can_filter.SlaveStartFilterBank = 14;

  if (hcan == &hcan1) {
    can_filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    can_filter.FilterBank = 0;
  } else if (hcan == &hcan2) {
    can_filter.FilterFIFOAssignment = CAN_FILTER_FIFO1;
    can_filter.FilterBank = 14;
  } else {
    return;
  }

  if (HAL_CAN_ConfigFilter(hcan, &can_filter) != HAL_OK) {
    Error_Handler();
  }
}

void CAN_Send_Msg(CAN_HandleTypeDef *hcan, uint8_t *msg, uint32_t id,
                  uint8_t len)
{
  CAN_TxHeaderTypeDef tx_header = {0};
  tx_header.StdId = id;
  tx_header.IDE = CAN_ID_STD;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = len;
  HAL_CAN_AddTxMessage(hcan, &tx_header, msg, &p_tx_mailbox);
}

/**
 * @brief   CAN1 FIFO0 接收中断回调
 * @param   hcan: CAN 句柄
 * @retval  None
 * @note    CAN1 上挂的是 yaw 电机(0x12) 和底盘板间帧(0x210)
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan == &hcan1) {
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header1,
                             can1_rx_data) == HAL_OK) {
      /* DM 反馈 ID = 电机 ID + 0x10：pitch(id=1)=0x11、yaw(id=2)=0x12
       * 两个 ID 都解码，接线万一换回来也不用改代码。 */
      uint32_t std_id = rx_header1.StdId;
      if (std_id == hello_world::motor::DaMiao::GetRxId(1)) {
        GetMotorPitch()->decode(can1_rx_data);
      } else if (std_id == hello_world::motor::DaMiao::GetRxId(2)) {
        GetMotorYaw()->decode(can1_rx_data);
      } else if (std_id == 0x210u) {
        /* 兼容：板间线也可能插在 CAN1 上，这里一并接收 */
        GetGimbalChassisComm()->decode(can1_rx_data);
      }
    }
  }
  HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/**
 * @brief   CAN2 FIFO1 接收中断回调
 * @param   hcan: CAN 句柄
 * @retval  None
 * @note    CAN2 上挂的是 pitch 电机(0x11)，另外也收底盘板间帧(0x210)
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan == &hcan2) {
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &rx_header2,
                             can2_rx_data) == HAL_OK) {
      /* 兼容：万一某台电机其实挂在 CAN2 上（接线和图不一致），这里也收 */
      if (rx_header2.StdId == hello_world::motor::DaMiao::GetRxId(1)) {
        GetMotorPitch()->decode(can2_rx_data);
      } else if (rx_header2.StdId == hello_world::motor::DaMiao::GetRxId(2)) {
        GetMotorYaw()->decode(can2_rx_data);
      } else if (rx_header2.StdId == 0x210u) {
        GetGimbalChassisComm()->decode(can2_rx_data);
      }
    }
  }
  HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
}
