# -*- coding: utf-8 -*-
"""把 C 源码字符串字面量里的品牌名换成宏。

规则：只在双引号字符串内部替换，把
      "前缀 Axiom NeuroBench 2003 后缀"
   变成
      "前缀 " SM_PRODUCT_NAME " 后缀"
   这样转义、格式符都原样保留。

用于 setup.c / NB-SP1.c 这类还没手工参数化的文件。
"""
import io
import sys

# 长的放前面，避免先被短串吃掉
RULES = [
    ("Axiom NeuroBench 2003", "SM_PRODUCT_NAME"),
    ("Axiom NeuroBench",      "SM_SHORT_NAME"),
    ("NeuroBench 2003",       "SM_SHORT_NAME"),
    ("Axiom Software",        "SM_COMPANY"),
]


def transform(src):
    out = []
    i = 0
    n = len(src)
    in_str = False
    changed = 0

    while i < n:
        c = src[i]

        if not in_str:
            if c == '"':
                in_str = True
            out.append(c)
            i += 1
            continue

        # 在字符串内部
        if c == "\\":                     # 转义：原样跳过两个字符
            out.append(src[i:i + 2])
            i += 2
            continue
        if c == '"':                      # 字符串结束
            in_str = False
            out.append(c)
            i += 1
            continue

        matched = False
        for word, macro in RULES:
            if src.startswith(word, i):
                out.append('" ' + macro + ' "')
                i += len(word)
                changed += 1
                matched = True
                break
        if not matched:
            out.append(c)
            i += 1

    return "".join(out), changed


def main():
    total = 0
    for p in sys.argv[1:]:
        src = io.open(p, encoding="utf-8").read()
        new, n = transform(src)
        if n:
            io.open(p, "w", encoding="utf-8", newline="\n").write(new)
        print("%-28s %d 处" % (p, n))
        total += n
    print("合计", total)


if __name__ == "__main__":
    main()
