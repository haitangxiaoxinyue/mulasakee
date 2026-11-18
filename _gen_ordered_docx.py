# -*- coding: utf-8 -*-
"""生成《调试与调参手册》Word 文档：按步骤顺序组织。"""

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

OUT = r"C:\Users\htxxy\Desktop\omni_skeleton (2)\omni_skeleton\调试与调参手册.docx"

doc = Document()
st = doc.styles["Normal"]
st.font.name = "微软雅黑"
st.font.size = Pt(10.5)
st.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")
for nm in ("Heading 1", "Title"):
    s = doc.styles[nm]
    s.font.name = "微软雅黑"
    s.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")
for sec in doc.sections:
    sec.left_margin = Cm(2.2); sec.right_margin = Cm(2.2)
    sec.top_margin = Cm(2.0); sec.bottom_margin = Cm(2.0)


def para(t, bold=False):
    p = doc.add_paragraph(); r = p.add_run(t); r.bold = bold; return p


def bullet(t):
    return doc.add_paragraph(t, style="List Bullet")


def table(headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"; t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        c = t.rows[0].cells[i]; c.text = ""
        r = c.paragraphs[0].add_run(h); r.bold = True; r.font.size = Pt(9.5)
    for row in rows:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = ""
            r = cells[i].paragraphs[0].add_run(str(v)); r.font.size = Pt(9.5)
    if widths:
        for row in t.rows:
            for i, w in enumerate(widths):
                row.cells[i].width = Cm(w)
    doc.add_paragraph(); return t


t0 = doc.add_heading("调试与调参手册", level=0)
t0.alignment = WD_ALIGN_PARAGRAPH.CENTER
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("omni_skeleton 双板步兵 · 按顺序做"); r.font.size = Pt(11)
r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)
para("")
para("每一步都写清楚：做什么、怎么判断成功、不成功改哪个参数。按顺序走，不要跳。", bold=True)

# ---------- 总览 ----------
doc.add_heading("总览：一共九步", level=1)
table(["步", "内容", "关键产物"],
      [["1", "烧录两板", "两块板同时重启"],
       ["2", "上电自检", "dbg_life_state = 2"],
       ["3", "标定 delta0", "前进直走"],
       ["4", "分离模式验证", "转 90 度推杆朝枪管走"],
       ["5", "跟随模式整定", "车头追上去停住"],
       ["6", "小陀螺整定", "原地匀速转、云台不跟转"],
       ["7", "pitch 机械限位", "推到底不堵转"],
       ["8", "PID 收尾", "yaw 不动，pitch 稳态误差小于 0.01"],
       ["9", "保护与看门狗演示", "急停、离线、拔线、复位"]],
      [1.0, 6.0, 11.0])

# ---------- 1 烧录 ----------
doc.add_heading("第 1 步  烧录", level=1)
table(["板", "烧这个", "Ozone 工程"],
      [["底盘", "Chassis\\build\\omni_chassis.elf", "Chassis\\chassis.jdebug"],
       ["云台", "Gimbal\\build\\omni_gimbal.elf", "Gimbal\\gimbal.jdebug"]],
      [1.6, 7.4, 8.0])
para("流程：架空 → 整车断电 → J-Link 接底盘 → 上电 → Download & Reset → 断电 5 秒 → "
     "J-Link 换云台 → 上电 → Download & Reset → 断电 5 秒 → 上电。")
para("只改了一块板也要走全程，两块板必须同时重新启动。烧完在 Ozone 里 Reload 一次。")

# ---------- 2 自检 ----------
doc.add_heading("第 2 步  上电自检（Ozone 接底盘）", level=1)
table(["变量", "应该是", "含义"],
      [["dbg_life_state", "2", "0=Dead 1=Resurrection 2=Working"],
       ["dbg_link_ok", "1", "双板通信正常"],
       ["dbg_gimbal_ready", "1", "云台两台电机都回过话"],
       ["dbg_imu_ready", "1", "IMU 自检通过、零漂收敛"]],
      [4.4, 2.2, 10.4])
para("四个都到齐才算进入 Working。有一个是 0 就先解决它，别往下调。")
para("上电约 1.2 秒后才进入 Working，这段时间推杆没反应是正常的。")

# ---------- 3 delta0 ----------
doc.add_heading("第 3 步  标定 delta0（最关键）", level=1)
para("位置：Chassis/RobotModules/component/robot.cpp    当前 kDeltaOffset = 0.0f")
para("它是什么：云台电机读到 0 时，枪管和车头正前方差多少度。填错，前进就会斜着走。")
bullet("1. 车轮架空，左拨杆拨到最上（分离），确认 dbg_mode = 0。")
bullet("2. 把枪管转到与车头中线平行，可用直尺贴车体侧面比。")
bullet("3. 手离开右摇杆，等云台停稳，读 dbg_delta。")
bullet("4. 读数大于 3.14 就减 6.28 再记。")
bullet("5. 把 kDeltaOffset 改成这个数，编译，只烧底盘。")
table(["现象", "怎么改"],
      [["推前进斜着走", "把残差再加进 kDeltaOffset，重烧"],
       ["往反方向偏", "kDeltaOffset 取负"],
       ["转云台 90 度推前进，车往右走", "delta 符号反，同样取负"]],
      [7.0, 11.0])

# ---------- 4 分离 ----------
doc.add_heading("第 4 步  分离模式验证（左拨杆上档）", level=1)
table(["动作", "正确表现", "不对怎么办"],
      [["云台摆正，推前进", "四个轮同向，车直走", "斜走就回第 3 步重标"],
       ["云台左转 90 度，推前进", "车朝枪管左边平移", "往右走，delta0 取负"],
       ["左摇杆左右", "原地转圈（横移不开放）", "正常，不用改"],
       ["松手", "车停住，不慢爬", "kRcDeadZone 0.05 → 0.08"]],
      [4.6, 8.0, 5.4])

# ---------- 5 跟随 ----------
doc.add_heading("第 5 步  跟随模式整定（左拨杆中档）", level=1)
para("原理：w = kFollowKp × delta；delta 小于死区给 0，超过时至少给 kFollowMinW，最大 kFollowWMax。")
para("理想：转云台，底盘追过去，追上就停，误差约 1 度。会有几度滞后，正常。")
table(["参数", "当前值", "什么时候改", "改成"],
      [["kFollowKp", "12.0", "滞后大", "16"],
       ["kFollowKp", "12.0", "冲过头来回摆", "8，还摆改 6"],
       ["kFollowWMax", "3.0", "追得太猛、车身甩", "2.0"],
       ["kFollowMinW", "0.3", "停在差几度不走", "0.5"],
       ["kFollowDeltaDz", "0.02", "到位后来回小抖", "0.03"]],
      [4.0, 2.4, 6.0, 5.6])
para("如果车头越追越远，是符号反了：kFollowKp 改成 -12.0。")

# ---------- 6 小陀螺 ----------
doc.add_heading("第 6 步  小陀螺整定（左拨杆下档）", level=1)
para("操作：左拨杆拨到最下，用右拇指滚遥控器上的拨轮（不是摇杆），松手回中就不转。")
table(["参数", "当前值", "作用", "怎么改"],
      [["kSpinWMax", "1.0", "最大自旋速度", "约 57 度每秒；更慢 0.6，更快 1.5"],
       ["kSpinDeadZone", "0.05", "拨轮死区", "拨了没反应就降到 0.02"]],
      [4.0, 2.4, 3.6, 8.0])
para("云台如果跟着车一起转，说明前馈没生效：检查 yaw_rate_ff 在 Spin 和 Follow 两个分支都有值。")

# ---------- 7 pitch 限位 ----------
doc.add_heading("第 7 步  pitch 机械限位", level=1)
para("两处一起改：底盘 kGimbalPitchSpan、云台 kPitchSpan，现在都是 0.45 rad。")
para("手推枪口到最上、最下，各读一次角度，取小的那一半，再留 10% 余量。")
para("判据：pitch 推到底不堵转、不出现一卡一卡。")

# ---------- 8 PID ----------
doc.add_heading("第 8 步  PID 收尾", level=1)
para("云台位置环在 Gimbal/Instance/component/ins_pid.cpp：", bold=True)
table(["轴", "kp", "ki", "kd", "限幅", "积分上限"],
      [["yaw", "20", "0", "0.5", "±7", "7"],
       ["pitch", "14", "10", "0.3", "±2.5", "100"]],
      [2.2, 2.2, 2.2, 2.2, 4.4, 4.8])
para("yaw 实测 0.13 度，不动。pitch 整定顺序：先 ki=0 只调 kp（10 起，每次 +4），再加 kd（0.2 起，每次 +0.1，不超过 0.6），最后加 ki（5 起，每次 +5）。")
para("判据：推杆 → 松手 → 等 5 秒 → 采样，稳态误差小于 0.01 rad。")
para("轮速 PID 在 Chassis/Instance/component/ins_pid.cpp，350 / 10 / 0.3，一般不动。")
para("注意：PID 类里 kd 被放大 1000 倍，kd 不要超过 1.0。")

# ---------- 9 保护 ----------
doc.add_heading("第 9 步  保护验证与看门狗演示", level=1)
table(["测试", "正确表现"],
      [["拨急停（右拨杆下）", "四轮立刻归零，dbg_life_state 回 0"],
       ["松开急停", "约 1.2 秒后回到 2"],
       ["关遥控器", "全停，dbg_rc_offline = 1"],
       ["拔板间 CAN 线", "两块板都停，dbg_link_ok = 0"]],
      [6.8, 11.2])
para("看门狗演示（要改代码，演示完必须改回来）：", bold=True)
bullet("1. 打开 Chassis/Task/component/comm_task.cpp，找到文件末尾的 if 块。")
bullet("2. 把 HAL_IWDG_Refresh(&hiwdg); 这一行前面加 // 注释掉。")
bullet("3. 编译 → 断电 → 烧底盘 → 断电 5 秒 → 上电。")
bullet("4. 现象：约 0.3 秒后板子自己复位，反复循环；Ozone 反复断开。")
bullet("5. 演示完把 // 删掉，重新编译烧录，恢复原样。")
para("不要用按暂停的方式来演示：调试脚本已经把看门狗在暂停时冻住了，暂停不会复位。")

# ---------- 验收 ----------
doc.add_heading("验收标准（七条）", level=1)
table(["#", "考官动作", "通过的判据"],
      [["1", "上电不碰", "车不动、云台不甩；约 1.2 秒后进入工作状态"],
       ["2", "推左摇杆", "前进后退直走；左右原地转"],
       ["3", "转云台 90 度再推前进", "车朝枪管方向平移"],
       ["4", "右摇杆", "云台两轴动、松手停住、到限位不顶死"],
       ["5", "中档：转云台", "车头追过去，停住不摆，误差 1 度内"],
       ["6", "下档：拨轮", "原地匀速自转，云台钉住，还能平移"],
       ["7", "急停 / 关遥控 / 拔线 / 屏蔽喂狗", "全部归零；屏蔽喂狗 0.3 秒复位"]],
      [1.0, 6.2, 10.8])

# ---------- 参数位置与调法 ----------
doc.add_heading("参数位置与调法（按文件）", level=1)
para("下表的行号对应当前这版代码。以后改过代码，行号会变，请按参数名搜索。", bold=True)

para("一、底盘 Chassis/RobotModules/component/robot.cpp")
table(["行", "参数", "当前值", "怎么调"],
      [["75", "kVxMax", "2.0", "前进最快速度，太快就降到 1.5"],
       ["76", "kVyMax", "2.0", "横移速度，现在没用到，不用管"],
       ["77", "kWMax", "5.0", "分离模式原地转最快，太猛就降到 3.0"],
       ["82", "kDt", "0.001", "控制周期，不要动"],
       ["83", "kYawRateMax", "2.0", "右摇杆推满时云台 yaw 转速，太慢加到 3.0"],
       ["84", "kPitchRateMax", "1.0", "右摇杆推满时 pitch 转速，太慢加到 1.5"],
       ["87", "kGimbalPitchSpan", "0.45", "pitch 软限位，必须和云台 kPitchSpan 一起改"],
       ["88", "kDeltaOffset", "0.0", "delta0，填标定时读到的 dbg_delta"],
       ["90", "kSelfCheckMs", "200", "自检保持时间，不用动"],
       ["93", "kRequireImu", "true", "应急开关，改 false 后必须改回来"],
       ["97", "kRcDeadZone", "0.05", "松手车还爬就改 0.08"],
       ["106", "kFollowKp", "12.0", "滞后大改 16；冲过头改 8 或 6；越追越远改 -12"],
       ["107", "kFollowWMax", "3.0", "追得太猛、车身甩就改 2.0"],
       ["108", "kFollowMinW", "0.3", "停在差几度不走就改 0.5"],
       ["109", "kFollowDeltaDz", "0.02", "到位后来回小抖就改 0.03"],
       ["110", "kSpinWMax", "1.0", "小陀螺最快转速，更慢 0.6，更快 1.5"],
       ["111", "kSpinDeadZone", "0.05", "拨轮推了没反应就改 0.02"]],
      [1.2, 4.4, 2.2, 10.2])

para("二、底盘其它文件")
table(["文件", "行", "参数", "当前值", "怎么调"],
      [["chassis.cpp", "51", "kR", "0.07786", "轮半径，实测值，别动"],
       ["chassis.cpp", "52", "kAB", "0.15338×2", "半轮距加半轴距，别动"],
       ["chassis.cpp", "64", "kMaxOmega", "40", "轮速限幅，明显变慢可加到 50"],
       ["ins_pid.cpp", "24-27", "轮速 PID", "350/10/0.3", "起步肉加到 450，啸叫发抖减到 250"],
       ["ins_motor.cpp", "24-27", "四个轮的方向", "Fwd/Fwd/Rev/Rev", "哪个轮反了只改那一行"],
       ["ins_comm.cpp", "—", "遥控离线阈值", "100 ms", "不要动"]],
      [3.6, 1.6, 3.6, 4.0, 7.2])

para("三、云台 Gimbal/")
table(["文件", "行", "参数", "当前值", "怎么调"],
      [["RobotModules/gimbal.cpp", "71", "kPitchSpan", "0.45", "与底盘 kGimbalPitchSpan 一起改"],
       ["RobotModules/gimbal.cpp", "74", "kYawDirSign", "1.0", "yaw 转反了改 -1.0"],
       ["RobotModules/gimbal.cpp", "75", "kPitchDirSign", "1.0", "pitch 转反了改 -1.0"],
       ["RobotModules/gimbal.cpp", "80", "kMaxTargetStep", "0.002", "云台跟得慢可加到 0.004"],
       ["Instance/ins_pid.cpp", "27", "yaw PID", "20/0/0.5，±7", "误差 0.13 度，别动"],
       ["Instance/ins_pid.cpp", "36", "pitch PID", "14/10/0.3，±2.5", "抖降 kd 到 0.2，慢加 kp 到 18，差加 ki 到 15"],
       ["Instance/ins_motor.cpp", "23-27", "yaw 电机", "ID2，Fwd，-1.36920", "零位不用手改，用 kDeltaOffset 补"],
       ["Instance/ins_motor.cpp", "28-32", "pitch 电机", "ID1，Rev，0.02480", "同上"]],
      [5.0, 1.6, 3.0, 3.8, 6.6])

para("四、共享 RobotComponents/")
table(["文件", "行", "参数", "当前值", "怎么调"],
      [["gimbal_chassis_comm.cpp", "19", "kAngleScale", "1000", "角度缩放，别动"],
       ["gimbal_chassis_comm.cpp", "20", "kOfflineThresMs", "100", "断联阈值，别动"]],
      [5.6, 1.4, 3.6, 2.6, 6.8])

para("能调的只有三类：手感类（速度、转速、死区、跟随增益），标定类（kDeltaOffset、两个 pitch 限位），"
     "方向类（kDirFwd/Rev、两个 DirSign）。kDt、kPi、kAngleScale、轮半径这些是物理或协议常量，不要碰。", bold=True)

# ---------- 记录 ----------
doc.add_heading("记录表", level=1)
table(["项目", "实测", "备注"],
      [["delta0 读数", "", ""],
       ["delta0 是否取负", "是 / 否", ""],
       ["跟随最终 kp", "", ""],
       ["跟随滞后角度", "", "转云台时车头差几度"],
       ["跟随稳态误差", "", "松手后停在差几度"],
       ["小陀螺一圈用时", "", "秒表"],
       ["pitch 上下极限", "", ""],
       ["其它异常", "", ""]],
      [5.0, 6.0, 7.0])

doc.save(OUT)
print("saved:", OUT)
d2 = Document(OUT)
print("tables:", len(d2.tables))
print("headings:", len([p for p in d2.paragraphs if p.style.name.startswith("Heading")]))
