# -*- coding: utf-8 -*-
"""生成《omni_skeleton 现场调试手册》Word 文档。"""

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

OUT = r"C:\Users\htxxy\Desktop\omni_skeleton (2)\omni_skeleton\现场调试手册.docx"

doc = Document()

# ---------- 全局字体 ----------
style = doc.styles["Normal"]
style.font.name = "微软雅黑"
style.font.size = Pt(10.5)
style.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

for name in ("Heading 1", "Heading 2", "Title"):
    st = doc.styles[name]
    st.font.name = "微软雅黑"
    st.element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

# 页边距
for sec in doc.sections:
    sec.left_margin = Cm(2.2)
    sec.right_margin = Cm(2.2)
    sec.top_margin = Cm(2.0)
    sec.bottom_margin = Cm(2.0)


def h1(text):
    p = doc.add_heading(text, level=1)
    return p


def h2(text):
    return doc.add_heading(text, level=2)


def para(text, bold=False, color=None):
    p = doc.add_paragraph()
    run = p.add_run(text)
    run.bold = bold
    if color:
        run.font.color.rgb = RGBColor(*color)
    return p


def bullet(text):
    p = doc.add_paragraph(text, style="List Bullet")
    return p


def table(headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    hdr = t.rows[0].cells
    for i, htext in enumerate(headers):
        hdr[i].text = ""
        r = hdr[i].paragraphs[0].add_run(htext)
        r.bold = True
        r.font.size = Pt(9.5)
    for row in rows:
        cells = t.add_row().cells
        for i, val in enumerate(row):
            cells[i].text = ""
            r = cells[i].paragraphs[0].add_run(str(val))
            r.font.size = Pt(9.5)
    if widths:
        for row in t.rows:
            for i, w in enumerate(widths):
                row.cells[i].width = Cm(w)
    doc.add_paragraph()
    return t


# ================= 封面 =================
title = doc.add_heading("omni_skeleton 现场调试手册", level=0)
title.alignment = WD_ALIGN_PARAGRAPH.CENTER
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("烧录 → 验证 → 标定 → 调参")
r.font.size = Pt(13)
r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("2026-11-11 版    底盘板 + 云台板")
r.font.size = Pt(9)

para("")
para("使用顺序：按第 1 章烧录，第 3 章一轮一轮验，第 4 章标定，第 5 章调参，最后第 7 章演练。",
     bold=True)

# ================= 1 烧什么 =================
h1("1  烧什么、怎么烧")

h2("1.1  两个固件文件")
table(
    ["板", "用 Ozone 烧", "同一固件（备用）", "Ozone 工程"],
    [
        ["底盘", "Chassis\\build\\omni_chassis.elf", "Chassis\\build\\Debug\\omni_chassis.hex / .bin", "Chassis\\chassis.jdebug"],
        ["云台", "Gimbal\\build\\omni_gimbal.elf", "Gimbal\\build\\Debug\\omni_gimbal.hex / .bin", "Gimbal\\gimbal.jdebug"],
    ],
    widths=[1.6, 5.6, 6.0, 4.2],
)
para("说明：.elf 是固件加调试符号，Ozone 点 Download & Reset Program 就烧进去；.hex / .bin 内容相同，"
     "给 STM32CubeProgrammer 或 J-Flash 用。注意 .elf 在 build 根下，.hex / .bin 在 build\\Debug 下。")

h2("1.2  烧录流程（10 步，一步都不能省）")
table(
    ["步", "做什么", "为什么"],
    [
        ["1", "车轮架空、云台让开机械行程", "烧录时板子会重启，电机可能瞬动"],
        ["2", "整车断电（拔电池），等 5 秒", "不断电就烧，另一块板会带着旧指令自己转"],
        ["3", "DAP-Link 接到底盘板 SWD（PA13 / PA14 / 3.3V / GND）", ""],
        ["4", "上电", "此时两块板都带电，手不要碰云台"],
        ["5", "Ozone 打开 chassis.jdebug，Download & Reset Program", "烧 omni_chassis.elf"],
        ["6", "断电 5 秒", "让底盘彻底停"],
        ["7", "DAP-Link 换到云台板 SWD", ""],
        ["8", "上电，Ozone 打开 gimbal.jdebug，Download & Reset Program", "烧 omni_gimbal.elf"],
        ["9", "断电 5 秒，再上电", "两块板必须同时重新开始"],
        ["10", "Ozone 里 Reload 一次", "挂着旧 elf 会读到天文数字"],
    ],
    widths=[1.0, 8.6, 7.6],
)
para("注意：只改了一块板也要走全程，另一块必须跟着断电重启，否则它记着“上一辆车”的状态。", bold=True)

h2("1.3  Ozone 找不到 elf")
bullet("先确认 build 目录里真的有 omni_chassis.elf / omni_gimbal.elf。")
bullet("确认打开的是对应板的 .jdebug，两块板别弄反。")
bullet(".jdebug 里用的是相对路径 $(ProjectDir)/build/xxx.elf，工程文件放哪都行。")

# ================= 2 上电先看 =================
h1("2  上电后先看这 5 个数（Ozone，底盘）")
table(
    ["变量", "正常值", "含义"],
    [
        ["dbg_life_state", "2", "0 = Dead，1 = Resurrection，2 = Working"],
        ["dbg_link_ok", "1", "双板通信正常"],
        ["dbg_gimbal_ready", "1", "云台两台电机都回过话"],
        ["dbg_imu_ready", "1", "IMU 自检通过且零漂收敛（约 1.2 秒后才变 1）"],
        ["dbg_mode", "0 / 1 / 2", "分离 / 跟随 / 小陀螺，跟左拨杆走"],
    ],
    widths=[4.2, 2.4, 10.4],
)
para("上电时序：Dead →（遥控在线 + 双板通 + 云台就绪）→ Resurrection →（等 IMU 1 秒 + 200 ms）→ Working，"
     "总共约 1.2 秒。这段时间推杆没反应是正常的。")

# ================= 3 验证 =================
h1("3  验证顺序（5 轮，每轮只验一件事）")

h2("第 1 轮  安全保护（车不动，只拨开关）")
table(
    ["#", "操作", "期望", "不对怎么办"],
    [
        ["1.1", "关遥控器", "dbg_rc_offline = 1，dbg_life_state = 0，四轮输出 0", "看 dbg_rc_offline 会不会变"],
        ["1.2", "开遥控器", "约 1.2 秒后 dbg_life_state = 2", "看第 2 章三张表"],
        ["1.3", "右拨杆拨下（急停）", "立刻 dbg_life_state = 0，dbg_wheel_target 全 0", "检查急停判断"],
        ["1.4", "松开急停", "约 1.2 秒后回到 2", ""],
        ["1.5", "拔掉板间 CAN 线", "dbg_link_ok = 0，立刻回 0，车停；云台也应停", "检查 isOffline 阈值"],
        ["1.6", "看门狗（可选）", "注释心跳那行后重烧，约 0.3 秒复位", "复位后记得改回来"],
    ],
    widths=[1.1, 5.2, 7.6, 4.1],
)
para("第 1 轮全过，才允许进第 2 轮。安全项不过，后面一律不测。", bold=True)

h2("第 2 轮  驾驶与 δ 符号（架空）")
table(
    ["#", "操作", "期望", "不对怎么办"],
    [
        ["2.1", "云台正对车头，推前", "四个轮全部正转", "—"],
        ["2.2", "云台转到车头左侧 90 度，推前", "LF 正、LB 反、RB 正、RF 反（向左平移）", "反向则 delta_ 取负"],
        ["2.3", "落地：云台转 90 度推前", "车朝云台指的方向走", "同上"],
    ],
    widths=[1.1, 5.6, 7.4, 3.9],
)

h2("第 3 轮  云台（架空、无负载）")
table(
    ["#", "操作", "期望", "不对怎么办"],
    [
        ["3.1", "右摇杆左右", "yaw 跟着转，方向对（推右转右）", "kYawDirSign 取负，或改 ins_motor 的 dir"],
        ["3.2", "松手", "停在原地不飘", "已加死区；仍飘看 kp 和积分"],
        ["3.3", "右摇杆上下", "pitch 上下，到极限不顶死", "kGimbalPitchSpan / kPitchSpan 太大"],
        ["3.4", "手推 pitch 到机械极限", "不堵转、不卡顿", "限位改小"],
    ],
    widths=[1.1, 5.2, 7.0, 4.7],
)

h2("第 4 轮  三模式")
table(
    ["#", "操作", "期望", "不对怎么办"],
    [
        ["4.1", "左拨杆 上 / 中 / 下", "dbg_mode = 0 / 1 / 2", "交换 kSwitchStateUp / Mid / Down 映射"],
        ["4.2", "中档，转云台", "车头追云台，最后停在 1 度内，不来回摆", "抖就降 kFollowKp；推不动就加 kFollowMinW"],
        ["4.3", "下档，拨轮给一点", "车原地转，云台钉住世界方向", "把 -= 改成 +="],
        ["4.4", "下档，拨轮回中", "不转（安全默认）", "想默认慢转改 kSpinWMax 那段"],
    ],
    widths=[1.1, 5.6, 7.2, 4.1],
)

# ================= 4 标定 =================
h1("4  要标定的 3 个量")
table(
    ["#", "标什么", "怎么量", "填到哪", "不标会怎样"],
    [
        ["4.1", "yaw 零位（可选）", "枪管对准车头中线，读 angle()", "Gimbal/Instance/ins_motor.cpp 的 angle_offset", "只是读数不好看，delta0 能兜住"],
        ["4.2", "delta0（必做）", "同上摆正，记下角度", "Chassis/RobotModules/robot.cpp 的 kDeltaOffset", "车会斜着走"],
        ["4.3", "pitch 机械限位（必做）", "手推到最上 / 最下各读一次", "底盘 kGimbalPitchSpan 和云台 kPitchSpan（两处一起改）", "顶限位，过载，一卡一卡"],
    ],
    widths=[1.0, 4.0, 5.4, 6.2, 4.0],
)
para("delta0 怎么填：摆正时读到 1.370，就写 kDeltaOffset = 1.370f。")
para("pitch 限位怎么填：上极限 +0.32、下极限 -0.28，取较小的 0.28，再留 10% 余量写 0.25。")

# ================= 5 调参 =================
h1("5  要调的参数（现场改这几个）")
table(
    ["#", "参数", "文件", "现值", "目标", "怎么调"],
    [
        ["5.1", "kFollowKp", "底盘 robot.cpp", "12.0", "5 度误差能推动车，最终停在 1 度内", "抖就降到 8，还抖降到 6"],
        ["5.2", "kFollowWMax", "底盘 robot.cpp", "3.0", "追 90 度约 0.5 到 1 秒", "觉得猛就降到 2.0"],
        ["5.3", "kFollowMinW", "底盘 robot.cpp", "0.3", "能克服静摩擦", "还推不动就加到 0.5"],
        ["5.4", "kFollowDeltaDz", "底盘 robot.cpp", "0.02", "到位不抖", "还抖就加到 0.03"],
        ["5.5", "kSpinWMax", "底盘 robot.cpp", "3.0", "转得稳不甩", "先 1.5"],
        ["5.6", "pitch PID", "云台 ins_pid.cpp", "14 / 10 / 0.3，±2.5", "稳态误差小于 0.01 rad", "推杆，松手等 5 秒再采样"],
        ["5.7", "yaw PID", "云台 ins_pid.cpp", "20 / 0 / 0.5，±7", "误差小于 0.15 度", "已达标，别动"],
        ["5.8", "轮速 PID", "底盘 ins_pid.cpp", "350 / 10 / 0.3", "轮速跟得上不抖", "先别动，队友经验值"],
        ["5.9", "kRcDeadZone", "底盘 robot.cpp", "0.05", "松手不漂", "漂移大就调到 0.08"],
        ["5.10", "kGimbalPitchSpan / kPitchSpan", "两处", "0.45", "按实测", "两处一起改"],
    ],
    widths=[1.0, 4.6, 3.6, 4.0, 4.2, 4.4],
)
para("调参铁律：一次只改一个数，改完重烧，记下改前改后的现象。", bold=True)

# ================= 6 排错 =================
h1("6  出问题怎么查")
table(
    ["症状", "最可能原因", "动作"],
    [
        ["上电推杆没反应", "还在 Dead / Resurrection", "看 dbg_life_state，等 1.2 秒"],
        ["dbg_imu_ready 一直 0", "片选、SPI、供电", "看 GetImu()->initErr()（要 0）和 dataOk()"],
        ["IMU 坏了但想先开", "—", "把 kRequireImu 改成 false 重烧，排查完改回来"],
        ["车不动，dbg_gimbal_ready = 0", "云台没回话", "云台板电、程序、CAN 线、电机 ID"],
        ["车不动，dbg_link_ok = 0", "收不到 0x211", "板间线要插在底盘 CAN1 和云台 CAN1"],
        ["推前走偏一个角度", "delta0 没标", "第 4.2 条"],
        ["转云台推杆往反方向横移", "δ 符号反", "delta_ 取负"],
        ["云台一卡一卡、一会硬一会软", "限幅太大或顶机械限位", "限幅降到 2.5，限位改小"],
        ["Ozone 读到大数（±8e37）", "挂了旧 elf", "重新加载与 hex 同批的 elf"],
        ["Ozone 搜不到变量", "打开了另一块板的工程", "确认 .jdebug"],
        ["上电就跑、乱转", "没断电就烧了", "按 1.2 重走"],
    ],
    widths=[5.4, 5.2, 8.0],
)

# ================= 7 演练 =================
h1("7  验收演练（照考官顺序）")
table(
    ["#", "考官动作", "你要做的", "状态"],
    [
        ["1", "上电不碰", "等 1.2 秒，车不动、云台不甩", "已具备"],
        ["2", "推杆，转云台 90 度再推", "车朝云台方向走", "待第 2 轮验证"],
        ["3", "右摇杆", "云台两轴动，松手保持", "已具备"],
        ["4", "中档转云台", "车头追云台", "待第 4 轮验证"],
        ["5", "下档拨轮", "自旋，云台钉住", "待第 4 轮验证"],
        ["6", "急停", "全部归零", "第 1 轮验证"],
        ["7", "关遥控 / 拔线 / 屏蔽喂狗", "归零 / 0.3 秒复位", "第 1 轮验证"],
    ],
    widths=[1.0, 5.6, 7.6, 4.4],
)

# ================= 8 记录 =================
h1("8  现场记录（回来照着改代码）")
table(
    ["项目", "实测值", "备注"],
    [
        ["yaw 摆正读数", "", ""],
        ["delta0 填入值", "", ""],
        ["δ 符号", "正 / 反", ""],
        ["pitch 上极限 / 下极限", "", ""],
        ["kPitchSpan 最终值", "", "两处都改"],
        ["kFollowKp 最终值", "", ""],
        ["kSpinWMax 最终值", "", ""],
        ["pitch 稳态误差", "", "松手 5 秒后采"],
        ["看门狗复位时间", "", "秒表"],
        ["其它异常", "", ""],
    ],
    widths=[5.6, 5.6, 7.4],
)

doc.save(OUT)
print("saved:", OUT)

# 验证
d2 = Document(OUT)
print("paragraphs:", len(d2.paragraphs), "tables:", len(d2.tables))
print("title:", d2.paragraphs[0].text if d2.paragraphs else "")
