/*
 * @Author: miunoki 13567781025@163.com
 * @Date: 2026-09-19 15:11:39
 * @LastEditors: miunoki 13567781025@163.com
 * @LastEditTime: 2026-09-19 21:14:38
 * @FilePath: \omni_skeleton\Chassis\Task\component\comm_task.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
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

/* Private variables ---------------------------------------------------------*/
/* 遥控器接收缓冲区（18 字节 DBUS，来源：inc/DT7.hpp 注释） */
static uint8_t rc_rx_buf[hello_world::remote_control::kRcRxDataLen];

/* 控制链心跳（定义在 main_task.cpp）：喂狗前先看它有没有涨 */
extern volatile uint32_t g_main_task_cnt;
static uint32_t s_last_main_cnt = 0;

/* ===== 调试用全局变量：CAN1（板间通信，与云台板相连）健康状态 ===== */
extern uint32_t dbg_can1_rx_total;   /* 定义在 src/HW_can.cpp，收到过多少帧 */
uint32_t dbg_can1_errorcode = 0;     /* HAL 累积错误码：0=从未出错，4=掉过总线 */
uint32_t dbg_can1_esr = 0;           /* ESR 寄存器：bit2=总线关闭，bit1=错误被动，bit0=错误警告 */

/* Exported function definitions ---------------------------------------------*/
void CommTaskInit(void) {
  /* 1. CAN 滤波器初始化（全收） */
  CanFilter_Init(&hcan1);
  CanFilter_Init(&hcan2);

  /* 2. 启动 CAN 总线（必须！漏了它电机收不到指令、反馈也收不回来）
   *    HAL_CAN_Init 只是「配置」外设，HAL_CAN_Start 才真正让 CAN 控制器开始收发 */
  HAL_CAN_Start(&hcan1);
  HAL_CAN_Start(&hcan2);

  /* 3. 激活 CAN 接收中断通知（必须！漏了它收不到电机反馈，回调不触发）
   *    hcan1 → FIFO0，hcan2 → FIFO1（与 CanFilter_Init 里分配的 FIFO 一致） */
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* 4. 启动遥控器串口空闲 + DMA 接收（USART3，18 字节 DBUS） */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rc_rx_buf,
                               hello_world::remote_control::kRcRxDataLen);

  /* 5. IWDG 已由 main.c 的 MX_IWDG_Init() 启动，这里刷一次 */
  HAL_IWDG_Refresh(&hiwdg);
}

void CommTask(void) {
  /* 1. 4 个电机 encode 进同一帧（id1~4 共用发送 ID 0x200），一次发出 */
  uint8_t data[8] = {0};
  for (int i = 0; i < 4; ++i) {
    GetMotorWheel(i)->encode(data);
  }
  CAN_Send_Msg(&hcan2, data, hello_world::motor::M3508::GetTxId(1), 8);

  /* 2. 双板通信：把底盘数据（在线标志 + 云台目标角）发给云台板
   *    走 CAN1，帧 ID 0x210（C2G） */
  uint8_t comm_data[8] = {0};
  GetGimbalChassisComm()->encode(comm_data);
  CAN_Send_Msg(&hcan1, comm_data, 0x210u, 8);
  /* 兼容：板间线也可能插在 CAN2 上，两个口都发一份 */
  CAN_Send_Msg(&hcan2, comm_data, 0x210u, 8);

  /* 3. 采集 CAN1 健康状态（调试用） */
  dbg_can1_errorcode = hcan1.ErrorCode;
  dbg_can1_esr       = hcan1.Instance->ESR;

  /* 4. 喂狗：只有控制链完成了一个新的周期才喂。
   *    MainTask 卡死 / 被临时屏蔽 → 计数不动 → 这里不喂 → 板子按 IWDG 超时复位。 */
  if (g_main_task_cnt != s_last_main_cnt) {
    s_last_main_cnt = g_main_task_cnt;
    HAL_IWDG_Refresh(&hiwdg);
  }
}

/* Private function definitions ----------------------------------------------*/
/* UART 空闲中断回调（C 链接，覆盖 HAL 弱函数） */
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                                           uint16_t Size) {
  if (huart == &huart3 &&
      Size >= hello_world::remote_control::kRcRxDataLen) {
    /* 收满 18 字节，解包遥控器 */
    GetRemoteControl()->decode(rc_rx_buf);
    GetRemoteOfflineChecker()->update();  // 标记：刚收到遥控数据，在线
  }
  /* 重新启动下一轮接收 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rc_rx_buf,
                               hello_world::remote_control::kRcRxDataLen);
}
