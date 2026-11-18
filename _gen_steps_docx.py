# -*- coding: utf-8 -*-
"""生成《现场调试步骤与验收标准》Word 文档。"""

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

OUT = r"C:\Users\htxxy\Desktop\omni_skeleton (2)\omni_skeleton\现场调试步骤与验收标准.docx"

doc = Document()

style = doc.styles["Normal"]
style.font.name = "微软雅黑"
style.font.size = Pt(10.5)
style.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")
for name in ("Heading 1", "Heading 2", "Title"):
    st = doc.styles[name]
    st.font.name = "微软雅黑"
    st.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

for sec in doc.sections:
    sec.left_margin = Cm(2.2)
    sec.right_margin = Cm(2.2)
    sec.top_margin = Cm(2.0)
    sec.bottom_margin = Cm(2.0)


def h1(t):
    return doc.add_heading(t, level=1)


def h2(t):
    return doc.add_heading(t, level=2)


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
    hdr = t.rows[0].cells
    for i, h in enumerate(headers):
        hdr[i].text = ""
        r = hdr[i].paragraphs[0].add_run(h)
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


# ---------------- 封面 ----------------
t = doc.add_heading("现场调试步骤与验收标准", level=0)
t.alignment = WD_ALIGN_PARAGRAPH.CENTER
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("omni_skeleton 双板步兵 · 2026-11-11")
r.font.size = Pt(11)
r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

para("")
para("这份文档只回答两件事：现在按什么顺序调；最后什么样才算调好。", bold=True)
para("每一节都写成「做什么 → 成功判据 → 不对怎么办」，照着走就行。")

# ---------------- 第 1 章 顺序 ----------------
h1("一、总顺序")
para("顺序不能颠倒，因为后一步依赖前一步的结果：")
table(
    ["步", "调什么", "为什么排在这"],
    [
        ["1", "烧录两板", "所有验证的前提"],
        ["2", "上电自检", "先确认系统在 Working，不然调什么都没意义"],
        ["3", "标定 delta0", "决定方向对不对，是后面一切的基础"],
        ["4", "分离模式验证", "最简单、最容易判断，用来确认 delta0 标对了"],
        ["5", "跟随模式三个参数", "依赖 delta0 和方向"],
        ["6", "小陀螺", "依赖 delta0 和方向"],
        ["7", "pitch 机械限位", "防止顶死，独立于上面几步"],
        ["8", "保护与复位", "急停、离线、拔线、看门狗"],
        ["9", "全套演练", "按考官顺序走一遍"],
    ],
    widths=[1.0, 6.0, 11.0],
)

# ---------------- 第 2 章 烧录 ----------------
h1("二、第 1 步：烧录")
para("两个固件：")
table(
    ["板", "烧这个", "Ozone 工程"],
    [
        ["底盘", "Chassis\\build\\omni_chassis.elf", "Chassis\\chassis.jdebug"],
        ["云台", "Gimbal\\build\\omni_gimbal.elf", "Gimbal\\gimbal.jdebug"],
    ],
    widths=[1.6, 7.4, 8.0],
)
para("流程：架空 → 整车断电 → J-Link 接底盘 → 上电 → Download & Reset → 断电 5 秒 → "
     "J-Link 换云台 → 上电 → Download & Reset → 断电 5 秒 → 上电。")
para("只改了一块板也要走全程，两块板必须同时重新启动。")

# ---------------- 第 3 章 自检 ----------------
h1("三、第 2 步：上电自检（Ozone 接底盘）")
para("上电后约 1.2 秒，看这四个数：")
table(
    ["变量", "应该是", "含义"],
    [
        ["dbg_life_state", "2", "0=Dead 1=Resurrection 2=Working"],
        ["dbg_link_ok", "1", "双板通信正常"],
        ["dbg_gimbal_ready", "1", "云台两台电机都回过话"],
        ["dbg_imu_ready", "1", "IMU 自检通过、零漂收敛"],
    ],
    widths=[4.4, 2.2, 10.4],
)
para("判据：四个都到齐才算进入 Working。有一个是 0，先解决它，别往下调。")
para("dbg_life_state 显示 out of scope 时，删掉这条 watch 重新加一次，或者看 dbg_mode 是否在变。")

# ---------------- 第 4 章 delta0 ----------------
h1("四、第 3 步：标定 delta0（最关键）")
para("delta0 是云台电机的零点和车体正前方之间的夹角。填错，前进就会斜着走。")
para("做法（全程架空、车不需要动）：")
bullet("1. 车轮架空，左拨杆拨到最上（分离），确认 dbg_mode = 0。")
bullet("2. 把云台转到枪管与车头中线平行。可用直尺贴车体侧面，让枪管与它平行。")
bullet("3. 手离开右摇杆，等云台停稳，读 dbg_delta。")
bullet("4. 如果读数大于 3.14，减掉 6.28 再记。")
bullet("5. 打开 Chassis/RobotModules/component/robot.cpp，找到 kDeltaOffset = 0.0f，把 0.0f 换成读到的数。")
bullet("6. 编译、断电、烧底盘、断电 5 秒、上电。")
para("判据：分离模式下，云台摆正，推前进，车直着走；云台转 90 度，推前进，车朝枪管方向平移。")
para("不对怎么办：还差一点就再读一次残差加进去；往反方向偏，说明符号反了，把 kDeltaOffset 取负。")

# ---------------- 第 5 章 分离 ----------------
h1("五、第 4 步：分离模式验证")
table(
    ["动作", "正确表现", "不对怎么办"],
    [
        ["云台摆正，推前进", "四个轮同向，车直走", "斜走，回第 3 步重标 delta0"],
        ["云台左转 90 度，推前进", "车朝枪管左边平移（左前右后正转，左后右前反转）", "往右走，delta0 符号反"],
        ["左摇杆左右", "原地转圈，不是横移（横移本设计不开放）", "正常，不用改"],
        ["松手", "车停住，不慢爬", "还爬就把 kRcDeadZone 从 0.05 加到 0.08"],
    ],
    widths=[4.6, 8.0, 5.4],
)

# ---------------- 第 6 章 跟随 ----------------
h1("六、第 5 步：跟随模式（左拨杆中档）")
para("原理：w = 12 × delta，delta 小于 0.02 时给 0，超过时至少给 0.3，最大 3.0。")
para("理想表现：转云台一个角度，底盘跟着追过去，追上就停，误差 1 度左右。会有几度滞后，属于正常。")
table(
    ["现象", "原因", "怎么改"],
    [
        ["轮子一直转、停不下来", "云台跟着底盘一起转，delta 永远不消", "已经修过：跟随也做了前馈补偿，重烧即可"],
        ["车头越追越远", "增益符号反", "kFollowKp 改成 -12.0"],
        ["追得慢、滞后大", "增益小", "kFollowKp 12 → 16"],
        ["冲过头来回摆", "增益大或有积分", "kFollowKp 降到 8，或 6"],
        ["停在差几度不走", "最小转速不够", "kFollowMinW 0.3 → 0.5"],
        ["到位后来回小抖", "死区太小", "kFollowDeltaDz 0.02 → 0.03"],
        ["追得太猛、甩", "限幅大", "kFollowWMax 3.0 → 2.0"],
    ],
    widths=[5.4, 6.6, 6.0],
)
para("判据：转云台 90 度，车头在 1 秒左右追平，停住不摆，误差 1 度内。")

# ---------------- 第 7 章 小陀螺 ----------------
h1("七、第 6 步：小陀螺（左拨杆下档）")
para("操作：右拇指滚遥控器上的拨轮（不是摇杆），往左滚一个方向自转，往右滚反向，松手回中就不转。")
para("当前速率上限 1.0 rad/s，约 57 度每秒，一圈约 6 秒。想更慢改 kSpinWMax。")
para("理想表现：车原地匀速自转；云台稳稳钉住一个方向，不跟着车转；这时推左摇杆还能平移，方向仍按枪管。")
table(
    ["现象", "原因", "怎么改"],
    [
        ["拨轮没反应", "拨轮值在死区里", "kSpinDeadZone 0.05 → 0.02"],
        ["转的方向与预期相反", "符号", "把 w = wheel * kSpinWMax 改成负号"],
        ["云台跟着车转", "前馈没生效", "检查 yaw_rate_ff 在 Spin 和 Follow 都生效"],
        ["转得太快", "速率上限", "kSpinWMax 1.0 → 0.6"],
    ],
    widths=[5.4, 6.6, 6.0],
)

# ---------------- 第 8 章 pitch ----------------
h1("八、第 7 步：pitch 机械限位")
para("现在 0.45 rad 是估的。手推枪口到最上、最下，各读一次角度，取较小的那一半，留 10% 余量。")
para("要改两处，必须一起改：底盘 kGimbalPitchSpan、云台 kPitchSpan。")
para("判据：pitch 推到底不堵转、不出现一卡一卡。")

# ---------------- 第 9 章 保护 ----------------
h1("九、第 8 步：保护与复位")
table(
    ["测试", "正确表现"],
    [
        ["拨急停（右拨杆下）", "四轮立刻归零，dbg_life_state 回 0"],
        ["松开急停", "约 1.2 秒后回到 2"],
        ["关遥控器", "全停，dbg_rc_offline = 1"],
        ["拔板间 CAN 线", "两块板都停，dbg_link_ok = 0"],
        ["注释掉心跳那行后重烧", "约 0.3 秒复位（看门狗验收演示）"],
    ],
    widths=[6.8, 11.2],
)

# ---------------- 第 10 章 验收标准 ----------------
h1("十、最终验收标准（七条）")
table(
    ["#", "考官动作", "通过的判据"],
    [
        ["1", "上电不碰", "车不动、云台不甩；约 1.2 秒后进入工作状态"],
        ["2", "推左摇杆", "前进后退直走；左摇杆左右原地转"],
        ["3", "转云台 90 度再推前进", "车朝枪管方向平移，不是朝车头"],
        ["4", "右摇杆", "云台 yaw 转、pitch 上下；松手停住不飘；到限位不顶死"],
        ["5", "中档：转云台", "车头追过去，停住不摆，误差 1 度内"],
        ["6", "下档：拨轮", "原地匀速自转，云台钉住不跟着转，还能平移"],
        ["7", "急停 / 关遥控 / 拔线 / 屏蔽喂狗", "全部归零；屏蔽喂狗 0.3 秒复位"],
    ],
    widths=[1.0, 6.2, 10.8],
)
para("七条全过 = 通过。再加模块封装清晰、Git 历史、PID 稳快无超调 = 良好。", bold=True)

# ---------------- 第 11 章 当前参数 ----------------
h1("十一、当前参数值（改之前先看这里）")
table(
    ["参数", "文件", "当前值", "作用"],
    [
        ["kDeltaOffset", "底盘 robot.cpp", "0.0（待标定）", "delta 零位"],
        ["kFollowKp", "底盘 robot.cpp", "12.0", "跟随增益"],
        ["kFollowWMax", "底盘 robot.cpp", "3.0", "跟随最大转速"],
        ["kFollowMinW", "底盘 robot.cpp", "0.3", "跟随最小转速"],
        ["kFollowDeltaDz", "底盘 robot.cpp", "0.02", "跟随死区"],
        ["kSpinWMax", "底盘 robot.cpp", "1.0", "小陀螺最大转速"],
        ["kSpinDeadZone", "底盘 robot.cpp", "0.05", "拨轮死区"],
        ["kRcDeadZone", "底盘 robot.cpp", "0.05", "摇杆死区"],
        ["kGimbalPitchSpan", "底盘 robot.cpp", "0.45", "pitch 限位（与云台同步改）"],
        ["kPitchSpan", "云台 gimbal.cpp", "0.45", "pitch 限位（与底盘同步改）"],
        ["kRequireImu", "底盘 robot.cpp", "true", "是否强制 IMU 就绪"],
        ["kSelfCheckMs", "底盘 robot.cpp", "200", "自检保持时长"],
    ],
    widths=[4.4, 4.2, 3.6, 8.0],
)

# ---------------- 第 12 章 记录 ----------------
h1("十二、记录表")
table(
    ["项目", "实测", "备注"],
    [
        ["delta0 读数", "", ""],
        ["delta0 是否取负", "是 / 否", ""],
        ["跟随最终 kp", "", ""],
        ["跟随滞后角度", "", "转云台时车头差几度"],
        ["跟随稳态误差", "", "松手后停在差几度"],
        ["小陀螺实际转速", "", "秒表算一圈时间"],
        ["pitch 上下极限", "", ""],
        ["其它异常", "", ""],
    ],
    widths=[5.0, 6.0, 7.0],
)

doc.save(OUT)
print("saved:", OUT)
d2 = Document(OUT)
print("tables:", len(d2.tables), "paras:", len(d2.paragraphs))
