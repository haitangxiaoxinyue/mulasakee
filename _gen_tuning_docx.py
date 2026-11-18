# -*- coding: utf-8 -*-
"""生成《参数整定手册》Word 文档：按“块”组织，不写总顺序。"""

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

OUT = r"C:\Users\htxxy\Desktop\omni_skeleton (2)\omni_skeleton\参数整定手册.docx"

doc = Document()
style = doc.styles["Normal"]
style.font.name = "微软雅黑"
style.font.size = Pt(10.5)
style.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")
for nm in ("Heading 1", "Heading 2", "Title"):
    st = doc.styles[nm]
    st.font.name = "微软雅黑"
    st.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")
for sec in doc.sections:
    sec.left_margin = Cm(2.2)
    sec.right_margin = Cm(2.2)
    sec.top_margin = Cm(2.0)
    sec.bottom_margin = Cm(2.0)


def para(t, bold=False):
    p = doc.add_paragraph()
    r = p.add_run(t)
    r.bold = bold
    return p


def bullet(t):
    return doc.add_paragraph(t, style="List Bullet")


def table(headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        c = t.rows[0].cells[i]
        c.text = ""
        r = c.paragraphs[0].add_run(h)
        r.bold = True
        r.font.size = Pt(9.5)
    for row in rows:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = ""
            r = cells[i].paragraphs[0].add_run(str(v))
            r.font.size = Pt(9.5)
    if widths:
        for row in t.rows:
            for i, w in enumerate(widths):
                row.cells[i].width = Cm(w)
    doc.add_paragraph()
    return t


title = doc.add_heading("参数整定手册", level=0)
title.alignment = WD_ALIGN_PARAGRAPH.CENTER
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("omni_skeleton 双板步兵 · 每一块单独怎么调")
r.font.size = Pt(11)
r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)
para("")
para("每一节独立：讲清楚这一块在哪里、现在多少、怎么判断好坏、怎么改。哪块有问题就翻哪块。", bold=True)

# 一、烧录
doc.add_heading("一、烧录", level=1)
table(["板", "烧这个", "Ozone 工程"],
      [["底盘", "Chassis\\build\\omni_chassis.elf", "Chassis\\chassis.jdebug"],
       ["云台", "Gimbal\\build\\omni_gimbal.elf", "Gimbal\\gimbal.jdebug"]],
      [1.6, 7.4, 8.0])
para("流程：架空 → 整车断电 → J-Link 接底盘 → 上电 → Download & Reset → 断电 5 秒 → "
     "J-Link 换云台 → 上电 → Download & Reset → 断电 5 秒 → 上电。")
para("只改了一块板也要走全程，两块板必须同时重新启动。")

# 二、上电自检
doc.add_heading("二、上电自检（Ozone 接底盘）", level=1)
table(["变量", "应该是", "含义"],
      [["dbg_life_state", "2", "0=Dead 1=Resurrection 2=Working"],
       ["dbg_link_ok", "1", "双板通信正常"],
       ["dbg_gimbal_ready", "1", "云台两台电机都回过话"],
       ["dbg_imu_ready", "1", "IMU 自检通过、零漂收敛"]],
      [4.4, 2.2, 10.4])
para("四个都到齐才算进入 Working。有一个是 0，先解决它，别往下调。")

# 三、delta0
doc.add_heading("三、云台零位夹角 delta0", level=1)
para("位置：Chassis/RobotModules/component/robot.cpp   当前值：kDeltaOffset = 0.0f")
para("它是什么：云台电机读到 0 的时候，枪管和车头正前方差多少度。填错，前进就会斜着走。")
para("怎么测：")
bullet("1. 车轮架空，左拨杆拨到最上（分离），确认 dbg_mode = 0。")
bullet("2. 把枪管转到与车头中线平行，用直尺贴车体侧面比。")
bullet("3. 手离开右摇杆，等云台停稳，读 dbg_delta。")
bullet("4. 读数大于 3.14 就减 6.28 再记。")
bullet("5. 把 kDeltaOffset 改成这个数，编译，只烧底盘。")
table(["现象", "怎么改"],
      [["推前进斜着走", "把残差再加进 kDeltaOffset，重烧"],
       ["往反方向偏", "kDeltaOffset 取负"],
       ["转云台 90 度推前进，车往右走", "delta 符号反，同样取负"]],
      [7.0, 11.0])

# 四、分离模式
doc.add_heading("四、分离模式验证", level=1)
table(["动作", "正确表现", "不对怎么办"],
      [["云台摆正，推前进", "四个轮同向，车直走", "斜走，回上一节重标 delta0"],
       ["云台左转 90 度，推前进", "车朝枪管左边平移", "往右走，delta0 取负"],
       ["左摇杆左右", "原地转圈（横移本设计不开放）", "正常，不用改"],
       ["松手", "车停住，不慢爬", "kRcDeadZone 0.05 → 0.08"]],
      [4.6, 8.0, 5.4])

# 五、跟随
doc.add_heading("五、跟随模式参数", level=1)
para("原理：w = kFollowKp × delta，delta 小于死区给 0，超过时至少给 kFollowMinW，最大 kFollowWMax。")
para("理想：转云台，底盘追过去，追上就停，误差约 1 度；会滞后几度，正常。")
table(["参数", "当前值", "作用", "什么时候改"],
      [["kFollowKp", "12.0", "跟随增益", "滞后大就加到 16；冲过头就降到 8 或 6"],
       ["kFollowWMax", "3.0", "最大转速", "追得太猛、车身甩就降到 2.0"],
       ["kFollowMinW", "0.3", "最小转速", "停在差几度不走就加到 0.5"],
       ["kFollowDeltaDz", "0.02", "到位死区", "到位后来回小抖就加到 0.03"]],
      [4.0, 2.4, 3.6, 8.0])
para("如果车头越追越远，是符号反了：kFollowKp 改成 -12.0。")

# 六、小陀螺
doc.add_heading("六、小陀螺参数", level=1)
para("操作：左拨杆拨到最下，用右拇指滚遥控器上的拨轮（不是摇杆），松手回中就不转。")
table(["参数", "当前值", "作用", "怎么改"],
      [["kSpinWMax", "1.0", "最大自旋速度", "约 57 度每秒，一圈 6 秒；更慢改 0.6，更快改 1.5"],
       ["kSpinDeadZone", "0.05", "拨轮死区", "拨了没反应就降到 0.02"]],
      [4.0, 2.4, 3.6, 8.0])
para("如果云台跟着车一起转，说明前馈没生效，检查 yaw_rate_ff 在 Spin 和 Follow 两个分支都要有值。")

# 七、云台 PID
doc.add_heading("七、云台位置环 PID（yaw / pitch）", level=1)
para("位置：Gimbal/Instance/component/ins_pid.cpp")
table(["轴", "kp", "ki", "kd", "限幅", "积分上限"],
      [["yaw", "20", "0", "0.5", "±7 N·m", "7"],
       ["pitch", "14", "10", "0.3", "±2.5 N·m", "100"]],
      [2.2, 2.2, 2.2, 2.2, 4.4, 4.8])
para("yaw 实测误差 0.13 度，已经很好，不建议动。")
para("pitch 整定步骤：")
bullet("1. 先把 ki 设为 0，只调 kp：从 10 开始，每次加 4，直到推杆时响应快、不抖。")
bullet("2. 加 kd：从 0.2 起，每次加 0.1，直到超调消失。kd 不要超过 0.6。")
bullet("3. 最后加 ki：从 5 起，每次加 5，直到稳态误差小于 0.01 rad。")
bullet("4. 判据：推杆 → 松手 → 等 5 秒 → 采样，稳态误差小于 0.01 rad。")
para("注意：输出限幅不要加大到 7，之前给满时顶到机械限位，电机过载自动清错重启，表现为一卡一卡。")

# 八、轮速 PID
doc.add_heading("八、轮速 PID", level=1)
para("位置：Chassis/Instance/component/ins_pid.cpp   当前 350 / 10 / 0.3，限幅 ±16384")
table(["现象", "怎么改"],
      [["起步肉、加速慢", "kp 加到 450"],
       ["轮子啸叫、车身抖", "kp 降到 250"],
       ["直线走偏", "检查四个轮参数是否一致，或某轮机械阻力大"]],
      [7.0, 11.0])
para("注意：这个 PID 类里 kd 被放大了 1000 倍（deriv 除以 0.001），kd 不要超过 1.0。")

# 九、pitch 限位
doc.add_heading("九、pitch 机械限位", level=1)
para("两处一起改：底盘 kGimbalPitchSpan、云台 kPitchSpan，现在都是 0.45 rad。")
para("怎么测：手推枪口到最上、最下，各读一次角度，取小的那一半，再留 10% 余量。")
para("判据：pitch 推到底不堵转、不出现一卡一卡。")

# 十、死区
doc.add_heading("十、摇杆与拨轮死区", level=1)
table(["参数", "当前值", "现象", "怎么改"],
      [["kRcDeadZone", "0.05", "松手车还在慢慢爬", "加到 0.08"],
       ["kSpinDeadZone", "0.05", "拨轮推了没反应", "降到 0.02"]],
      [4.0, 2.4, 6.0, 5.6])

# 十一、保护
doc.add_heading("十一、保护与复位验证", level=1)
table(["测试", "正确表现"],
      [["拨急停（右拨杆下）", "四轮立刻归零，dbg_life_state 回 0"],
       ["松开急停", "约 1.2 秒后回到 2"],
       ["关遥控器", "全停，dbg_rc_offline = 1"],
       ["拔板间 CAN 线", "两块板都停，dbg_link_ok = 0"],
       ["注释掉心跳那行后重烧", "约 0.3 秒复位（看门狗演示）"]],
      [6.8, 11.2])

# 十二、验收标准
doc.add_heading("十二、最终验收标准（七条）", level=1)
table(["#", "考官动作", "通过的判据"],
      [["1", "上电不碰", "车不动、云台不甩；约 1.2 秒后进入工作状态"],
       ["2", "推左摇杆", "前进后退直走；左右原地转"],
       ["3", "转云台 90 度再推前进", "车朝枪管方向平移，不是朝车头"],
       ["4", "右摇杆", "云台 yaw 转、pitch 上下；松手停住；到限位不顶死"],
       ["5", "中档：转云台", "车头追过去，停住不摆，误差 1 度内"],
       ["6", "下档：拨轮", "原地匀速自转，云台钉住不跟转，还能平移"],
       ["7", "急停 / 关遥控 / 拔线 / 屏蔽喂狗", "全部归零；屏蔽喂狗 0.3 秒复位"]],
      [1.0, 6.2, 10.8])
para("七条全过 = 通过。再加封装清晰、Git 历史、PID 稳快无超调 = 良好。", bold=True)

# 十三、参数总表
doc.add_heading("十三、当前参数总表", level=1)
table(["参数", "文件", "当前值"],
      [["kDeltaOffset", "底盘 robot.cpp", "0.0（待标定）"],
       ["kFollowKp", "底盘 robot.cpp", "12.0"],
       ["kFollowWMax", "底盘 robot.cpp", "3.0"],
       ["kFollowMinW", "底盘 robot.cpp", "0.3"],
       ["kFollowDeltaDz", "底盘 robot.cpp", "0.02"],
       ["kSpinWMax", "底盘 robot.cpp", "1.0"],
       ["kSpinDeadZone", "底盘 robot.cpp", "0.05"],
       ["kRcDeadZone", "底盘 robot.cpp", "0.05"],
       ["kGimbalPitchSpan", "底盘 robot.cpp", "0.45"],
       ["kPitchSpan", "云台 gimbal.cpp", "0.45"],
       ["yaw PID", "云台 ins_pid.cpp", "20 / 0 / 0.5，±7"],
       ["pitch PID", "云台 ins_pid.cpp", "14 / 10 / 0.3，±2.5"],
       ["轮速 PID", "底盘 ins_pid.cpp", "350 / 10 / 0.3，±16384"]],
      [4.4, 5.6, 8.0])

# 十四、记录
doc.add_heading("十四、记录表", level=1)
table(["项目", "实测", "备注"],
      [["delta0 读数", "", ""],
       ["delta0 是否取负", "是 / 否", ""],
       ["跟随最终 kp", "", ""],
       ["跟随滞后角度", "", "转云台时车头差几度"],
       ["跟随稳态误差", "", "松手后停在差几度"],
       ["小陀螺一圈用时", "", "秒表"],
       ["pitch 上下极限", "", ""],
       ["yaw 误差", "", ""],
       ["其它异常", "", ""]],
      [5.0, 6.0, 7.0])

doc.save(OUT)
print("saved:", OUT)
d2 = Document(OUT)
print("tables:", len(d2.tables), "paras:", len(d2.paragraphs))
print("headings:", [p.text for p in d2.paragraphs if p.style.name.startswith("Heading")])
