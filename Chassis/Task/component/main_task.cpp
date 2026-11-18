/**
 *******************************************************************************
 * @file      :main_task.cpp
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
#include "main_task.hpp"
#include "comm_task.hpp"
#include "imu.hpp"
#include "ins_all.hpp"
#include "robot.hpp"
#include "tim.h"

/* Exported function definitions ---------------------------------------------*/
/* 控制链心跳：每跑完一次 MainTask 自增一次。
 * CommTask 只在它变化时喂狗 → MainTask 卡死 / 被屏蔽，狗就不喂（看门狗验收用）。 */
volatile uint32_t g_main_task_cnt = 0;

void MainTaskInit(void) {
  /* 1. IMU 一次性初始化（配 BMI088 + Mahony；内部有阻塞，必须在这里做） */
  GetImu()->init();

  /* 2. 把各模块注册进 Robot（一次性，main() 里 while(1) 之前调用） */
  robot::Robot *robot = GetRobot();
  robot->registerChassis(GetChassis());//底盘
  robot->registerImu(GetImu());        //IMU（状态机的自检闸门）
  robot->registerRemoteControl(GetRemoteControl());//遥控器
  robot->registerGimbalChassisComm(GetGimbalChassisComm());//双板通信
  for (int i = 0; i < 4; ++i) {
    robot->registerMotorWheel(i, GetMotorWheel(i));//四个电机
    robot->registerPidWheel(i, GetPidMotorWheel(i));//四个pid
  }

  /* 通讯层初始化（CAN 滤波器 / UART DMA 接收 / IWDG） */
  CommTaskInit();
}

void MainTask(void) {
  /* 1ms 控制周期入口：先更新 IMU（读芯片 → 扣零漂 → 解算姿态），再跑状态机 */
  GetImu()->update();
  GetRobot()->update();
  /* C++20 里对 volatile 变量用 ++ 已弃用，改写成自增表达式（效果相同） */
  g_main_task_cnt = g_main_task_cnt + 1;
}

/* Private function definitions ----------------------------------------------*/
/* TIM6 周期中断回调（C 链接，覆盖 HAL 弱函数），每 1ms 触发一次 */
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim == &htim6) {
    MainTask();   // 控制：读遥控器 → 逆解 → PID → 电机
    CommTask();   // 通信：电机 encode → CAN 发送 + IWDG 刷新
  }
}
