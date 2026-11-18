/**
 *******************************************************************************
 * @file      :ins_motor.cpp
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
#include "ins_motor.hpp"
/* Private macro -------------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/

hello_world::motor::M3508 motor_wheel_[4] = {
 hello_world::motor::M3508(1,hello_world::motor:: OptionalParams{.dir =hello_world::motor:: kDirRev}),//左前   
 hello_world::motor::M3508(2, hello_world::motor::OptionalParams{.dir = hello_world::motor::kDirFwd}),//右前    
 hello_world::motor::M3508(3,hello_world::motor:: OptionalParams{.dir = hello_world::motor::kDirFwd}),//右后   
 hello_world::motor::M3508(4, hello_world::motor::OptionalParams{.dir = hello_world::motor::kDirRev}),//左后   
};
/* Private function prototypes -----------------------------------------------*/
/* Exported function definitions ---------------------------------------------*/
hello_world::motor::Motor *GetMotorWheel(int idx) {
  /* TODO【新人任务】:
   * 返回第 idx 号轮电机（M3508）单例。
   *
   * 你要做：
   *   1. 静态定义 4 个轮电机对象并存入数组（可用 hello_world::motor::CreateMotor
   *      (hello_world::motor::MotorType::kM3508, id, opt) 工厂，或直接构造 M3508）。
   *   2. 校验 idx ∈ [0,4)，返回对应元素的地址。
   *   3. 每个电机的 CAN id、方向、减速比按实机接线配置（见考核说明文档）。
   *
   * 约束 / 提示：
   *   - 电机设备类已提供：inc/motor_M3508.hpp（M3508）、inc/motor.hpp（CreateMotor 工厂、
   *     OptionalParams 配置项）。
   */
  /*M3508 构造函数长这样：

M3508(uint8_t id, const OptionalParams &opt = OptionalParams());

OptionalParams 里有个 dir 字段，两个取值：

- kDirFwd（=1，同向，默认值）
- kDirRev（=-1，反向）

4 个轮子都是反向，所以要传一个 OptionalParams，把 dir 设成 kDirRev。*/



 


    if (idx < 0 || idx >= 4) return nullptr; 
    return &motor_wheel_[idx];                 



 
}

/* Private function definitions ----------------------------------------------*/
