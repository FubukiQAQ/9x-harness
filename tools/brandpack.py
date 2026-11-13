#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""NeuroBench 品牌打包器（宿主机器上的可视化打包工具）

   双击 BrandPack.cmd，或：
       python tools\\brandpack.py

功能：
   * 图形化编辑 brand.ini（保留原文件的注释与未知字段）
   * 选图标、改产品名 / 公司 / 版本 / 安装目录 / 光盘卷标
   * 一键生成品牌头（快）或完整编译发行包，实时显示编译日志
   * 编译完成后可直接打开输出目录
"""

import io
import os
import re
import subprocess
import sys
import threading
import queue

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BRAND = os.path.join(ROOT, "brand.ini")

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except Exception:                                    # 没有 tkinter 时给出人话
    sys.stderr.write("需要 tkinter：请用带 Tk 的 Python 运行本工具。\n")
    raise

UI_FONT = ("MS Sans Serif", 8) if sys.platform == "win32" else ("TkDefaultFont", 9)
UI_FONT_B = ("MS Sans Serif", 8, "bold") if sys.platform == "win32" else ("TkDefaultFont", 9, "bold")
MONO = ("Consolas", 9) if sys.platform == "win32" else ("TkFixedFont", 9)

# 表单字段：(section, key, 中文标签, 宽度类别)
FIELDS = [
    ("product", "name",          "产品名",          "wide"),
    ("product", "name_en",       "产品名（英文）",  "wide"),
    ("product", "short",         "短名",            "wide"),
    ("product", "short_en",      "短名（英文）",    "wide"),
    ("product", "nickname",      "自称",            "wide"),
    ("product", "nickname_en",   "自称（英文）",    "wide"),
    ("product", "description",   "一句话说明",      "wide"),
    ("product", "description_en", "说明（英文）",   "wide"),
    ("product", "version",       "版本",            "narrow"),
    ("product", "version_tuple", "版本数字",        "narrow"),
    ("company", "name",          "公司 / 作者",     "wide"),
    ("company", "name_en",       "公司（英文）",    "wide"),
    ("company", "copyright",     "版权声明",        "wide"),
    ("company", "website",       "网址",            "narrow"),
    ("layout",  "exe",           "可执行文件名",    "narrow"),
    ("layout",  "ini_name",      "配置文件名",      "narrow"),
    ("layout",  "reg_root",      "注册表根键",      "wide"),
    ("layout",  "install_subdir", "安装子目录",     "wide"),
    ("layout",  "menu_group",    "开始菜单分组",    "wide"),
    ("layout",  "slug",          "文件名前缀",      "narrow"),
    ("layout",  "iso_label",     "光盘卷标",        "narrow"),
    ("layout",  "iso_label_tools", "工具盘卷标",    "narrow"),
]

ICON_FIELDS = [
    ("icons", "agent",  "主程序"),
    ("icons", "setup",  "安装程序"),
    ("icons", "keygen", "序列号生成器"),
    ("icons", "uninst", "卸载程序"),
]

VI_FIELDS = [
    ("versioninfo", "lang",     "语言块"),
    ("versioninfo", "codepage", "代码页"),
    ("versioninfo", "comments", "备注"),
]

GROUPS = [
    ("产品", [f for f in FIELDS if f[0] == "product"]),
    ("公司", [f for f in FIELDS if f[0] == "company"]),
    ("布局与文件名", [f for f in FIELDS if f[0] == "layout"]),
]


# ---------------------------------------------------------------------------
# brand.ini 读写（保留注释）
# ---------------------------------------------------------------------------
def read_ini(path):
    vals = {}
    order = []
    sec = None
    if not os.path.exists(path):
        return vals, order
    with io.open(path, "r", encoding="utf-8-sig") as f:
        for line in f:
            s = line.strip()
            if s.startswith("[") and s.endswith("]"):
                sec = s[1:-1].strip().lower()
            elif "=" in s and sec and not s.startswith(";"):
                k, v = s.split("=", 1)
                vals[(sec, k.strip().lower())] = v.strip()
                order.append((sec, k.strip().lower()))
    return vals, order


def write_ini(path, vals):
    """就地改值，保留注释、空行、未知字段与原有顺序。"""
    if not os.path.exists(path):
        with io.open(path, "w", encoding="utf-8", newline="\n") as f:
            for sec in ("product", "company", "layout", "icons", "versioninfo"):
                f.write("[%s]\n" % sec)
                for (s, k), v in sorted(vals.items()):
                    if s == sec:
                        f.write("%s = %s\n" % (k, v))
                f.write("\n")
        return

    lines = io.open(path, "r", encoding="utf-8-sig").read().split("\n")
    sec = None
    seen = set()
    out = []
    keyre = re.compile(r"^(\s*)([A-Za-z0-9_.-]+)(\s*=\s*)(.*)$")

    for line in lines:
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            sec = s[1:-1].strip().lower()
            out.append(line)
            continue
        m = keyre.match(line)
        if m and sec and not s.startswith(";"):
            k = m.group(2).lower()
            if (sec, k) in vals:
                # 保留行尾注释（前面有空格的 ; ）
                tail = ""
                v = vals[(sec, k)]
                old_rest = m.group(4)
                pos = old_rest.find(" ;")
                if pos >= 0:
                    tail = old_rest[pos:]
                out.append("%s%s%s%s%s" % (m.group(1), m.group(2), m.group(3), v, tail))
                seen.add((sec, k))
                continue
        out.append(line)

    # 补上文件里原来没有的键
    missing = [((s, k), v) for (s, k), v in vals.items() if (s, k) not in seen]
    if missing:
        bysec = {}
        for (s, k), v in missing:
            bysec.setdefault(s, []).append((k, v))
        out.append("")
        for s, items in bysec.items():
            out.append("[%s]" % s)
            for k, v in items:
                out.append("%s = %s" % (k, v))

    io.open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out))


def validate(vals):
    errs = []
    if not vals.get(("product", "name")):
        errs.append("产品名不能为空")
    vt = vals.get(("product", "version_tuple"), "")
    parts = [p.strip() for p in vt.split(",")]
    if len(parts) != 4 or not all(p.isdigit() for p in parts):
        errs.append("版本数字必须是 4 个用逗号分隔的数字，例如 3,0,0,2003")
    elif any(int(p) > 65535 for p in parts):
        errs.append("版本数字每段不能超过 65535")
    exe = vals.get(("layout", "exe"), "")
    if not exe:
        errs.append("可执行文件名不能为空")
    elif re.search(r'[\\/:*?"<>|\s]', exe):
        errs.append("可执行文件名不能含空格或 \\ / : * ? \" < > |")
    for sec, key, label in ICON_FIELDS:
        p = vals.get((sec, key), "")
        if p and not os.path.exists(os.path.join(ROOT, p)):
            errs.append("图标不存在：%s（%s）" % (p, label))
    return errs


# ---------------------------------------------------------------------------
# 界面
# ---------------------------------------------------------------------------
class App(object):
    def __init__(self, root):
        self.root = root
        self.vars = {}
        self.brand_path = tk.StringVar(value=DEFAULT_BRAND)
        self.status = tk.StringVar(value="就绪")
        self.quick = tk.BooleanVar(value=False)
        self.build_queue = queue.Queue()
        self.building = False

        root.title("NeuroBench 品牌打包器")
        root.minsize(760, 620)
        root.option_add("*Font", UI_FONT)
        try:
            root.iconbitmap(default=os.path.join(ROOT, "src", "agent", "res", "agent.ico"))
        except Exception:
            pass

        self.build_top()
        self.build_form()
        self.build_bottom()
        self.load_file(DEFAULT_BRAND)
        # 别让窗口比屏幕还高
        self.root.update_idletasks()
        w = max(760, self.root.winfo_reqwidth())
        h = min(self.root.winfo_reqheight(), int(self.root.winfo_screenheight() * 0.92))
        self.root.geometry("%dx%d" % (w, h))

    # ---------- 顶部 ----------
    def build_top(self):
        f = tk.Frame(self.root, padx=8, pady=6)
        f.pack(fill="x")
        tk.Label(f, text="品牌文件：", font=UI_FONT_B).pack(side="left")
        e = tk.Entry(f, textvariable=self.brand_path, width=52)
        e.pack(side="left", fill="x", expand=True, padx=(0, 6))
        tk.Button(f, text="打开…", width=8, command=self.on_open).pack(side="left", padx=2)
        tk.Button(f, text="另存为…", width=9, command=self.on_save_as).pack(side="left", padx=2)
        tk.Button(f, text="载入默认", width=9, command=lambda: self.load_file(DEFAULT_BRAND)).pack(side="left", padx=2)

    # ---------- 表单 ----------
    def build_form(self):
        outer = tk.Frame(self.root, padx=8)
        outer.pack(fill="both", expand=False)

        for title, fields in GROUPS:
            box = tk.LabelFrame(outer, text=" " + title + " ", padx=6, pady=4)
            box.pack(fill="x", pady=(0, 6))
            r = 0
            c = 0
            for sec, key, label, kind in fields:
                span = 2 if kind == "wide" else 1
                if kind == "wide":
                    if c != 0:
                        r += 1
                        c = 0
                tk.Label(box, text=label + "：", anchor="e", width=13).grid(
                    row=r, column=c * 2, sticky="e", pady=1)
                var = tk.StringVar()
                self.vars[(sec, key)] = var
                w = 44 if kind == "wide" else 22
                tk.Entry(box, textvariable=var, width=w).grid(
                    row=r, column=c * 2 + 1, columnspan=1, sticky="w", padx=(0, 10), pady=1)
                if kind == "wide":
                    r += 1
                else:
                    c += 1
                    if c > 1:
                        r += 1
                        c = 0

        # 图标
        box = tk.LabelFrame(outer, text=" 图标 ", padx=6, pady=4)
        box.pack(fill="x", pady=(0, 6))
        for i, (sec, key, label) in enumerate(ICON_FIELDS):
            tk.Label(box, text=label + "：", anchor="e", width=13).grid(row=i, column=0, sticky="e", pady=1)
            var = tk.StringVar()
            self.vars[(sec, key)] = var
            tk.Entry(box, textvariable=var, width=54).grid(row=i, column=1, sticky="w", padx=(0, 6))
            tk.Button(box, text="浏览…", width=7,
                      command=lambda k=(sec, key): self.pick_icon(k)).grid(row=i, column=2, sticky="w")

        # 版本信息
        box = tk.LabelFrame(outer, text=" 版本信息资源 ", padx=6, pady=4)
        box.pack(fill="x", pady=(0, 6))
        for i, (sec, key, label) in enumerate(VI_FIELDS):
            tk.Label(box, text=label + "：", anchor="e", width=13).grid(row=i, column=0, sticky="e", pady=1)
            var = tk.StringVar()
            self.vars[(sec, key)] = var
            tk.Entry(box, textvariable=var, width=54 if key == "comments" else 22).grid(
                row=i, column=1, sticky="w", padx=(0, 6))

    # ---------- 底部 ----------
    def build_bottom(self):
        bar = tk.Frame(self.root, padx=8, pady=4)
        bar.pack(fill="x")
        tk.Checkbutton(bar, text="只编译主程序（快，用于验证品牌）",
                       variable=self.quick).pack(side="left")
        tk.Button(bar, text="生成品牌头", width=12, command=self.on_gen_only).pack(side="right", padx=3)
        self.btn_build = tk.Button(bar, text="开始编译", width=12, command=self.on_build)
        self.btn_build.pack(side="right", padx=3)
        tk.Button(bar, text="保存配置", width=10, command=self.on_save).pack(side="right", padx=3)
        tk.Button(bar, text="打开输出目录", width=12, command=self.open_dist).pack(side="right", padx=3)

        logf = tk.Frame(self.root, padx=8)
        logf.pack(fill="both", expand=True, pady=(0, 6))
        self.log = tk.Text(logf, height=8, font=MONO, wrap="none",
                           bg="#FFFFFF", relief="sunken", borderwidth=1)
        sb = tk.Scrollbar(logf, command=self.log.yview)
        self.log.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.log.pack(side="left", fill="both", expand=True)

        st = tk.Frame(self.root, relief="sunken", borderwidth=1)
        st.pack(fill="x", side="bottom")
        tk.Label(st, textvariable=self.status, anchor="w", padx=6).pack(fill="x")

    # ---------- 事件 ----------
    def pick_icon(self, key):
        p = filedialog.askopenfilename(
            title="选择图标", filetypes=[("图标", "*.ico"), ("所有文件", "*.*")])
        if p:
            rel = os.path.relpath(p, ROOT).replace("/", "\\")
            self.vars[key].set(rel)

    def load_file(self, path):
        if not path or not os.path.exists(path):
            messagebox.showerror("载入失败", "找不到品牌文件：\n%s" % path)
            return
        vals, _ = read_ini(path)
        # 先用内置默认补齐，避免旧文件缺字段时输入框空着
        self.fill_from(vals)
        self.brand_path.set(path)
        self.status.set("已载入 %s（%d 个字段）" % (os.path.basename(path), len(vals)))
        self.log_write("载入品牌文件: %s" % path)

    def fill_from(self, vals):
        dflt = read_ini(DEFAULT_BRAND)[0]
        for (sec, key), var in self.vars.items():
            v = vals.get((sec, key))
            if v is None:
                v = dflt.get((sec, key), "")
            var.set(v)

    def collect(self):
        vals = {}
        for k, var in self.vars.items():
            vals[k] = var.get().strip()
        return vals

    def on_open(self):
        p = filedialog.askopenfilename(
            title="打开品牌文件", initialdir=ROOT,
            filetypes=[("品牌配置", "*.ini"), ("所有文件", "*.*")])
        if p:
            self.load_file(p)

    def on_save(self):
        vals = self.collect()
        errs = validate(vals)
        if errs:
            messagebox.showwarning("配置有问题", "\n".join(errs))
            return False
        write_ini(self.brand_path.get(), vals)
        self.status.set("已保存 %s" % os.path.basename(self.brand_path.get()))
        self.log_write("保存: %s" % self.brand_path.get())
        return True

    def on_save_as(self):
        p = filedialog.asksaveasfilename(
            title="另存品牌文件", initialdir=ROOT, defaultextension=".ini",
            filetypes=[("品牌配置", "*.ini")])
        if not p:
            return
        self.brand_path.set(p)
        if self.on_save():
            messagebox.showinfo("已保存", "品牌配置已保存到：\n%s" % p)

    def open_dist(self):
        d = os.path.join(ROOT, "build", "dist")
        if os.path.isdir(d):
            os.startfile(d) if sys.platform == "win32" else subprocess.Popen(["xdg-open", d])
        else:
            messagebox.showinfo("还没有产物", "先编译一次吧。")

    # ---------- 编译 ----------
    def on_gen_only(self):
        if not self.on_save():
            return
        self.run_async([sys.executable, os.path.join(ROOT, "tools", "mkbrand.py"),
                        self.brand_path.get()], "生成品牌头")

    def on_build(self):
        if not self.on_save():
            return
        target = "agent" if self.quick.get() else "release"
        ps = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
              "-File", os.path.join(ROOT, "build.ps1"),
              "-Target", target, "-Brand", self.brand_path.get()]
        self.run_async(ps, "编译（%s）" % target)

    def run_async(self, argv, title):
        if self.building:
            messagebox.showinfo("正在编译", "上一次编译还没结束，请稍候。")
            return
        self.building = True
        self.btn_build.configure(state="disabled")
        self.status.set(title + " 进行中…")
        self.log_write("\n===== %s =====" % title)
        self.log_write("$ " + " ".join(argv))

        env = dict(os.environ)
        env["PYTHONIOENCODING"] = "utf-8"

        def worker():
            try:
                p = subprocess.Popen(argv, cwd=ROOT, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, env=env)
                for raw in iter(p.stdout.readline, b""):
                    try:
                        line = raw.decode("utf-8")
                    except UnicodeDecodeError:
                        line = raw.decode("gbk", "replace")
                    self.build_queue.put(("line", line.rstrip("\r\n")))
                p.wait()
                self.build_queue.put(("done", p.returncode))
            except Exception as e:                      # noqa: BLE001
                self.build_queue.put(("line", "启动失败: %r" % (e,)))
                self.build_queue.put(("done", -1))

        threading.Thread(target=worker, daemon=True).start()
        self.root.after(100, self.poll_build)

    def poll_build(self):
        try:
            while True:
                kind, payload = self.build_queue.get_nowait()
                if kind == "line":
                    self.log_write(payload)
                elif kind == "done":
                    self.building = False
                    self.btn_build.configure(state="normal")
                    if payload == 0:
                        self.status.set("编译完成")
                        self.log_write("===== 完成 =====")
                    else:
                        self.status.set("编译失败（退出码 %s）" % payload)
                        self.log_write("===== 失败，退出码 %s =====" % payload)
        except queue.Empty:
            pass
        if self.building:
            self.root.after(150, self.poll_build)

    def log_write(self, s):
        self.log.insert("end", s + "\n")
        self.log.see("end")


def selftest():
    """无界面自检：读写 + 校验 + 生成品牌头。"""
    import tempfile
    src = DEFAULT_BRAND
    vals, _ = read_ini(src)
    print("读取 %d 个字段" % len(vals))
    errs = validate(vals)
    print("校验:", "通过" if not errs else errs)
    tmp = os.path.join(tempfile.gettempdir(), "brandpack_selftest.ini")
    write_ini(tmp, vals)
    vals2, _ = read_ini(tmp)
    diff = {k: (vals.get(k), vals2.get(k)) for k in vals if vals.get(k) != vals2.get(k)}
    print("往返一致:", not diff and len(vals2) == len(vals), diff or "")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkbrand.py"), tmp],
                       capture_output=True)
    print("生成品牌头:", "成功" if r.returncode == 0 else r.stderr.decode("utf-8", "replace"))
    return 0


def testbuild():
    """无界面跑一遍「编译」按钮的流程。"""
    root = tk.Tk()
    app = App(root)
    app.quick.set(True)          # 只编译主程序，快
    logf = os.path.join(ROOT, "build", "pack_log.txt")

    def kick():
        app.on_build()

    def watch():
        if app.building:
            root.after(300, watch)
        else:
            try:
                io.open(logf, "w", encoding="utf-8", newline="\n").write(app.log.get("1.0", "end"))
            except Exception:
                pass
            root.quit()

    root.after(300, kick)
    root.after(1200, watch)
    root.after(600000, root.quit)
    root.mainloop()
    print("状态:", app.status.get())
    print("日志:", logf)
    return 0 if app.status.get().startswith("编译完成") else 1


def main():
    if "--selftest" in sys.argv:
        return selftest()
    if "--testbuild" in sys.argv:
        return testbuild()
    root = tk.Tk()
    App(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
