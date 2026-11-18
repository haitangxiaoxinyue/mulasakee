#include "pid.hpp"                          // ← ① include

namespace {                                 // ← ③ 匿名命名空间：只在本文件可见
constexpr float kSampleTime = 0.001f;       // 1kHz 采样周期
}

namespace hello_world {                     // ← ② 命名空间
namespace pid {


Pid::Pid(float kp, float ki, float kd, float out_max, float out_min, float integ_max)
    : kp_(kp), ki_(ki), kd_(kd),
      out_max_(out_max), out_min_(out_min), integ_max_(integ_max),
      integ_(0.0f), last_err_(0.0f) {}





void Pid::setParams(float kp, float ki, float kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}
void Pid::setOutputLimit(float out_max, float out_min) {
    out_max_ = out_max;
    out_min_ = out_min;
    
}

void Pid::setIntegLimit(float integ_max) {
    integ_max_ = integ_max;
   
    
}

void Pid::reset (){
    integ_ = 0;
    last_err_ = 0;
}
float Pid::calc(float ref, float fdb) {
    float err   = ref - fdb;          // ① 误差
    float deriv = err - last_err_;    // ② 误差的变化量
    integ_ += err;                    // ③ 积分累加
    
    if (integ_ > integ_max_) { integ_ = integ_max_;  }   // ⑤ 积分限幅
    else if (integ_ < -integ_max_) {integ_ = -integ_max_; }
    
    float out = kp_ * err
              + ki_ * integ_ * kSampleTime
              + kd_ * deriv / kSampleTime;   // ④ 三项相加
    if (out > out_max_) { out = out_max_; integ_ -= err; }   // ⑤ 限幅+回退
    else if (out < out_min_) { out = out_min_; integ_ -= err; }
    
    
    last_err_ = err; 
                     // ⑥ 记住这拍误差
    return out;
}

}  // namespace pid
}  // namespace hello_world