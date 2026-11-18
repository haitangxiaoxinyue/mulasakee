# -*- coding: utf-8 -*-
"""生成《现在要调的参数》——只列还需要现场整定的项。"""

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

OUT = r"C:\Users\htxxy\Desktop\omni_skeleton (2)\omni_skeleton\现在要调的参数.docx"

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


t0 = doc.add_heading("现在要调的参数", level=0)
t0.alignment = WD_ALIGN_PARAGRAPH.CENTER
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("只列还没调好、现场需要动的"); r.font.size = Pt(11)
r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)
para("")
para("轮速 PID 你已经调过，yaw PID 实测 0.13 度也已经够好，这两个不在表里。", bold=True)
para("行号对应当前代码；改过代码后按参数名搜。")

# ---------------- 一、标定 ----------------
doc.add_heading("一、标定类（先做这三项）", level=1)
table(["参数", "文件", "行", "怎么调"],
      [["kDeltaOffset", "底盘 RobotModules/component/robot.cpp", "88",
        "架空、分离模式、枪管对准车头中线，读 dbg_delta，把这个数填进去。重烧底盘。"],
       ["kGimbalPitchSpan", "底盘 RobotModules/component/robot.cpp", "87",
        "手推 pitch 到最上、最下各读一次角度，取小的那一半再留 10% 余量。"],
       ["kPitchSpan", "云台 RobotModules/component/gimbal.cpp", "71",
        "和上面那个填一样的值，两处必须一起改。"]],
      [3.4, 5.6, 1.2, 8.8])
para("delta0 判断标准：分离模式、云台摆正、推前进，车直着走；云台转 90 度推前进，车朝枪管方向平移。")

# ---------------- 二、跟随 ----------------
doc.add_heading("二、跟随模式（底盘 robot.cpp 第 106 到 109 行）", level=1)
table(["参数", "行", "当前值", "什么时候改", "改成"],
      [["kFollowKp", "106", "12.0", "滞后大，车头明显跟不上", "16"],
       ["kFollowKp", "106", "12.0", "冲过头、来回摆", "8，还摆改 6"],
       ["kFollowKp", "106", "12.0", "越追越远（方向反）", "-12"],
       ["kFollowWMax", "107", "3.0", "追得太猛、车身甩", "2.0"],
       ["kFollowMinW", "108", "0.3", "停在差几度不走、轮子不转", "0.5"],
       ["kFollowDeltaDz", "109", "0.02", "到位后来回小抖", "0.03"]],
      [3.8, 1.2, 2.6, 7.2, 4.2])
para("判断标准：转云台 90 度，车头 1 秒左右追上，停住不摆，误差 1 度以内。")

# ---------------- 三、小陀螺 ----------------
doc.add_heading("三、小陀螺（底盘 robot.cpp 第 110 到 111 行）", level=1)
table(["参数", "行", "当前值", "什么时候改", "改成"],
      [["kSpinWMax", "110", "1.0", "转得太快或太慢", "更慢 0.6，更快 1.5"],
       ["kSpinDeadZone", "111", "0.05", "拨轮推了没反应", "0.02"]],
      [3.8, 1.2, 2.6, 7.2, 4.2])
para("操作：左拨杆最下档，滚遥控器上的拨轮（不是摇杆），松手回中就不转。")

# ---------------- 四、死区 ----------------
doc.add_heading("四、摇杆死区（底盘 robot.cpp 第 97 行）", level=1)
para("kRcDeadZone 现在是 0.05。松手以后车还在慢慢爬，就改成 0.08。")

# ---------------- 五、pitch PID ----------------
doc.add_heading("五、云台 pitch PID（云台 Instance/component/ins_pid.cpp 第 36 行）", level=1)
para("现在是 14 / 10 / 0.3，输出限幅 ±2.5，积分上限 100。")
table(["现象", "怎么改"],
      [["来回抖、啸叫", "kd 从 0.3 降到 0.2"],
       ["到位慢、跟不动", "kp 从 14 加到 18"],
       ["停稳后总差一点", "ki 从 10 加到 15"],
       ["推到底一卡一卡", "别加限幅，去检查 pitch 机械限位（第一节）"]],
      [7.0, 11.0])
para("判断标准：推杆、松手、等 5 秒再采样，稳态误差小于 0.01 rad。")

# ---------------- 六、不用动 ----------------
doc.add_heading("六、这些不用动", level=1)
table(["项", "位置", "原因"],
      [["轮速 PID 350/10/0.3", "底盘 Instance/component/ins_pid.cpp 第 24 到 27 行", "你已经调过"],
       ["yaw PID 20/0/0.5", "云台 Instance/component/ins_pid.cpp 第 27 行", "实测 0.13 度"],
       ["四个轮的方向", "底盘 Instance/component/ins_motor.cpp 第 24 到 27 行", "已经跑对了"],
       ["两台电机的零位", "云台 Instance/component/ins_motor.cpp 第 23 到 32 行", "用 kDeltaOffset 补"],
       ["kDt、kPi、kAngleScale、轮半径、kAB", "各处", "物理或协议常量"]],
      [5.4, 7.6, 5.0])

# ---------------- 七、记录 ----------------
doc.add_heading("七、记录", level=1)
table(["项目", "实测", "备注"],
      [["delta0 读数", "", ""],
       ["跟随最终 kp", "", ""],
       ["跟随滞后角度", "", "转云台时车头差几度"],
       ["跟随稳态误差", "", "松手后停在差几度"],
       ["小陀螺一圈用时", "", "秒表"],
       ["pitch 上下极限", "", ""]],
      [5.0, 6.0, 7.0])

doc.save(OUT)
print("saved:", OUT)
d2 = Document(OUT)
print("tables:", len(d2.tables), "headings:", len([p for p in d2.paragraphs if p.style.name.startswith("Heading")]))
