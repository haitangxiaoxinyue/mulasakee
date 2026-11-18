#ifndef PID_HPP_
#define PID_HPP_

namespace hello_world {
namespace pid {

class Pid {
public:
    Pid(float kp, float ki, float kd, float out_max, float out_min, float integ_max);
    void setParams(float kp, float ki, float kd);   // 改系数
    void setOutputLimit(float out_max, float out_min); // 改输出上下限
    void reset();                                    // 清零
    float calc(float ref, float fdb); 
    void setIntegLimit(float integ_max);               

private:
    float kp_, ki_, kd_;       // 三个系数
    float out_max_, out_min_;  // 输出的上限、下限
    float integ_max_;          // 积分限幅上限
    float integ_;              // 误差累积
    float last_err_;           // 上一拍误差
};

}  // namespace pid
}  // namespace hello_world

#endif /* PID_HPP_ */
