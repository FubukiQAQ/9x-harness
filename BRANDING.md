# 自定义打包器（品牌定制）

一套源码，可以打出多个“不同产品”。改 `brand.ini`，重新编译即可。

## 两种用法

### 1) 可视化打包器（推荐，在宿主机上跑）

双击仓库根目录的 **`BrandPack.cmd`**（或 `python tools\brandpack.py`）。

![界面] 左边是分组表单，右边是日志。

- 顶部选品牌文件：`打开…` / `另存为…` / `载入默认`
- 表单里改产品名、公司、版本、安装目录、光盘卷标
- 图标四行各带 `浏览…`，选 .ico 即可
- **生成品牌头**：只跑 `mkbrand.py`，一秒出结果（改完名字想马上看效果）
- **开始编译**：跑 `build.ps1 -Brand <当前文件>`，日志实时滚在下面
- 勾选 **只编译主程序** 可跳过安装盘，几秒钟完成，适合验证品牌
- **打开输出目录**：直接跳 `build\dist`

写回文件时会**保留原来的注释和未知字段**，只改值。

### 2) 命令行

```powershell
# 默认品牌（brand.ini）
powershell -File .\build.ps1 -Target release

# 另一套品牌
powershell -File .\build.ps1 -Target release -Brand brand-demo.ini

# 只编译主程序（快速验证品牌改动）
powershell -File .\build.ps1 -Target agent -Brand brand-demo.ini
```

产物在 `build\dist\`，文件名前缀取自 `[layout] slug`。

## brand.ini 里能配什么

| 字段 | 影响 |
|---|---|
| `product.name` / `name_en` | 窗口标题、关于框、欢迎语、系统提示词里的人设、卸载显示名 |
| `product.short` / `short_en` | 菜单项、对话框标题、开始菜单项、快捷方式名 |
| `product.nickname` | 系统提示词里“中文名称「…」” |
| `product.description` | 安装程序、版本信息里的 FileDescription |
| `product.version` / `version_tuple` | 标题、INI、版本信息资源、安装盘 |
| `company.*` | 关于框、版权、默认安装目录、注册表键、卸载发布者 |
| `layout.exe` | 可执行文件名（安装、卸载、快捷方式、工具包都会认） |
| `layout.ini_name` | 配置文件名 |
| `layout.reg_root` | 注册表根键（**换品牌后旧授权不通用，属预期**） |
| `layout.install_subdir` | 默认安装到 `Program Files\` 下的子目录 |
| `layout.menu_group` | 开始菜单分组 |
| `layout.slug` | 发行文件名前缀：`<slug>-DISK1-INSTALL.iso` 等 |
| `layout.iso_label` / `iso_label_tools` | 光盘卷标 |
| `icons.*` | 各程序图标（主程序 / 安装器 / 序列号生成器 / 卸载器） |
| `versioninfo.*` | 版本信息资源的语言块与注释 |

## 生成物（不要手改，编译时自动覆盖）

`tools/mkbrand.py` 会写出：

| 文件 | 用途 |
|---|---|
| `src/common/nb_brand.h` | C 侧宏：`SM_PRODUCT_NAME` / `SM_COMPANY` / `SM_VERSION_STR` / `SM_EXE_NAME` / `SM_INSTALL_SUBDIR` … |
| `src/common/brand_res.h` | 资源脚本用的图标绝对路径（GBK） |
| `src/common/brand_rc.inc` | VERSIONINFO 版本信息资源块（GBK） |
| `src/common/brand_summary.json` | 给 `build.ps1` 与 `mkdist.py` 读的机器摘要 |

## 改动原理

1. `sm.h` 不再硬编码产品信息，改为 `#include "nb_brand.h"`。
2. 各个 `.rc` 不再写死图标与版本信息，改为 `#include "../../common/brand_res.h"` 与 `brand_rc.inc`；
   `BRAND_INTERNAL` / `BRAND_ORIGNAME` / `BRAND_FILEDESC` 可按程序覆盖。
3. `tools/brandstr.py` 可以把某个 C 文件字符串字面量里的旧品牌名批量换成宏
   （只在双引号内替换，转义与格式符不受影响）：

   ```powershell
   python tools\brandstr.py src\setup\setup.c
   ```

4. 发行盘：`mkdist.py` 读取 `brand_summary.json`，生成的所有文本（README/EULA/SETUP.INS/DISK.ID）
   会做一次品牌字符串替换，文件名与 ISO 卷标用品牌值，并在打包前清理上一次的产物。

## 打包器的自检

```powershell
python tools\brandpack.py --selftest     # 读写往返 + 校验 + 生成品牌头
python tools\brandpack.py --testbuild    # 真正跑一遍“开始编译”，日志写 build\pack_log.txt
```

## 注意

- `product.version_tuple` 必须是 4 个数字，且每个 ≤ 65535。
- 图标必须是 `.ico`；PNG 不会被 windres 接受。
- 中文字符串沿用 GBK 输出（windres 用 `--codepage=936`）。
- `sm_lic.c` 里的 CDK 密钥是固定的，换品牌不会换算法——不同品牌共用同一套序列号算法。
- 可视化打包器只是宿主机工具，**不会**被打进发行盘。
