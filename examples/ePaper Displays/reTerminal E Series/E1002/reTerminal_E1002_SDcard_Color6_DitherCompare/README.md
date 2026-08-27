# reTerminal E1002 SD 卡图片新旧抖动算法对比（DitherCompare）

在 7.5 英寸 6 色电子墨水屏（800×480，ED2208）上，把**同一张图片**分别用新库（Seeed_GFX2 `dither/Dither.h`）和旧库算法（原 Seeed_GFX 的 Floyd-Steinberg 副本）渲染并屏显对比，新库一侧支持**全部参数串口实时调参**。

## 显示布局（左右分屏）

| 区域 | 列范围 | 内容 |
|---|---|---|
| 左半屏 | 0–398 | **新库**渲染结果（受串口参数控制） |
| 分隔线 | 399–400 | 2 像素黑线 |
| 右半屏 | 401–799 | **旧库**渲染结果（固定基线，公平对比用） |

两侧抖动的都是**同一张完整的半屏源图**（399×480 缩放副本），而不是把一张大图裁成两半，保证对比公平。

三种显示模式（串口命令）：

- `c` —— 左右分屏对比（默认）
- `a` —— 全屏新库（整幅 800×480）
- `b` —— 全屏旧库（整幅 800×480）

## 串口命令速查（Serial Command Reference）

| 按键 Key | 参数 Parameter | 示例 Example | 范围 Range | 默认 Default |
|----------|---------------|-------------|-----------|-------------|
| `?` | 打印所有参数 / Print all | `?` | — | — |
| `a` | 全屏新库 / Full NEW | `a` | — | — |
| `b` | 全屏旧库 / Full LEGACY | `b` | — | — |
| `c` | 分屏对比 / Split compare | `c` | — | — |
| `g` | 伽马 / Gamma | `g1.1` | 0.5~2.0 | 1.0 |
| `s` | 饱和度 / Saturation | `s0.25` | 0.0~1.0 | 0.0 |
| `d` | 暗度 / Darkness | `d0.08` | 0.0~0.5 | 0.0 |
| `t` | 对比度 / Contrast | `t1.15` | 0.5~2.0 | 1.0 |
| `e` | 扩散强度 / Diffusion | `e0.85` | 0.0~1.0 | 1.0 |
| `w` | 色温 / Warmth | `w0.20` | -1.0~1.0 | 0.0 |
| `l` | 钳位模式 / Clamp | `l` | 开/关 | 关（宽） |
| `p` | 蛇形扫描 / Serpentine | `p` | 开/关 | 开 |
| `m0` | FS（Floyd-Steinberg） | `m0` | — | ✓ |
| `m1` | ATKINSON | `m1` | — | |
| `m2` | BURKES | `m2` | — | |
| `m3` | SIERRA3 | `m3` | — | |
| `m4` | BAYER8（有序抖动） | `m4` | — | |
| `m5` | NONE（最近色，无抖动） | `m5` | — | |
| `m6` | PALETTE_MIX（双色混合） | `m6` | — | |
| `k` | 自定义调色板开关 / Custom palette | `k` | 开/关 | 关 |
| `k0`~`k5` | 编辑调色板颜色 / Edit palette | `k1 35 178 80` | R G B 0~255 | |

**支持连续输入**：一行可以输入多个命令，用空格分隔，例如 `w0.2 t0.6 s0.3 d0.05`，会依次执行每个命令并自动刷新画面。

> 参数修改只影响新库（左半屏 / `a` 全屏）；旧库固定为 FS、gamma=1.0、[0,255] 窄钳位、无蛇形，保持公平基线。

各参数的作用、分场景推荐值见同目录 **PARAM_GUIDE.md**。

## 各参数作用说明（Parameter Effect Summary）

| 参数 Parameter | 作用 Effect | 何时调大 When to increase | 何时调小 When to decrease |
|------|------|------------|------------|
| **Gamma** 伽马 | 整体亮度曲线 | 画面偏暗看不清细节 | 画面过曝/发白 |
| **Saturation** 饱和度 | 颜色鲜艳程度 | 色彩暗淡不鲜艳 | 颜色溢出/出现色块 |
| **Darkness** 暗度 | 抖动前预压暗 | 电子纸显示偏亮发灰 | 暗部死黑无细节 |
| **Contrast** 对比度 | 明暗层次分离 | 画面灰蒙蒙层次不清 | 高光过曝/暗部死黑 |
| **Diffusion** 扩散强度 | 误差传播比例 | 需要平滑渐变过渡 | 想要干净少噪点 |
| **Warmth** 色温 | 红蓝通道平衡 | 强调暖色调（落日等） | 强调冷色调（水面/蓝天） |
| **Clamp** 钳位 | 误差缓冲范围 | 纯色色块边缘更干净（`l` 开） | 照片渐变过渡更自然（`l` 关） |
| **Serpentine** 蛇形 | 之字形扫描 | 减少线条伪影 | 几何图案（`p` 关） |

## SD 卡准备

1. SD 卡格式化为 FAT32，创建 `/img/` 目录并放入图片（JPG/BMP/PNG，按魔数识别格式）。
2. 默认读取 `/img/demo.jpg`；找不到时自动扫描 `/img/` 和根目录，取第一个支持的图片。

## 自定义调色板

默认使用理论 sRGB 参考值。按 `k` 打开自定义调色板后可用 `k1 R G B` 等命令现场改色；也可以直接编辑代码顶部的 `CUSTOM_PALETTE` 数组，填入你自己面板的实测值。库内置 E6 调色板已是校准值，只有你面板的实测值才值得覆盖。

## 硬件说明

- 产品：reTerminal E1002，XIAO ESP32-S3 + 7.5 英寸 ED2208 6 色 ePaper，800×480
- EPD CS=GPIO10，DC=GPIO11，RST=GPIO12，BUSY=GPIO13
- SD 卡：共享 SPI，SCK=7，MISO=8，MOSI=9，CS=GPIO14，EN=GPIO16，DET=GPIO15

## 文件说明

- `reTerminal_E1002_SDcard_Color6_DitherCompare.ino` -- 主 sketch
- `legacy_dither.cpp` / `legacy_dither.h` -- 旧库算法的独立副本（对比基线）
- `image_loader.cpp` / `image_loader.h` -- SD 卡图片解码器（JPG/BMP/PNG）
- `pngle.c/h`、`miniz.c/h` -- PNG 解码依赖
- `PARAM_GUIDE.md` -- 参数调优指南（分场景推荐参数）

