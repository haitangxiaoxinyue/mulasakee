/**
 *******************************************************************************
 * @file      :ins_pid.cpp
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
#include "ins_pid.hpp"
#include "pid.hpp"
/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
namespace {
hello_world::pid::Pid motor_wheel_pid_[4] = {
    hello_world::pid::Pid(400.0f, 10.0f, 0.3f, 16384.0f, -16384.0f, 16384.0f),
    hello_world::pid::Pid(400.0f, 10.0f, 0.3f, 16384.0f, -16384.0f, 16384.0f),
    hello_world::pid::Pid(400.0f, 10.0f, 0.3f, 16384.0f, -16384.0f, 16384.0f),
    hello_world::pid::Pid(400.0f, 10.0f, 0.3f, 16384.0f, -16384.0f, 16384.0f),
};
}  // namespace
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
hello_world::pid::Pid *GetPidMotorWheel(int idx) {
    if (idx < 0 || idx >= 4) return nullptr;
    return &motor_wheel_pid_[idx];
}

/* Private function definitions ----------------------------------------------*/
