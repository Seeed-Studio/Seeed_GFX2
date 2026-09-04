# reTerminal E1004 SD 卡图片 6 色显示（新 Dither API）

将 SD 卡上的 JPG/BMP/PNG 图片显示到 reTerminal E1004 的 13.3 英寸 6 色电子墨水屏（1200×1600，T133A01，竖屏）。使用统一的 `dither/Dither.h` 模块和 `DitherConfig` API，以编译期常量暴露完整的抖动参数集，支持 7 种抖动/量化方法。

## 文件说明

- `reTerminal_E1004_SDcard_Color6.ino` -- 主 sketch
- `image_loader.h` / `image_loader.cpp` -- SD 卡图片解码器（JPG/BMP/PNG）
- `pngle.h` / `pngle.c` / `miniz.h` / `miniz.c` -- PNG 解码依赖

## 使用步骤

1. 将 SD 卡格式化为 FAT32，创建 `/img/` 目录，放入图片。
2. 修改 sketch 顶部的 `IMAGE_PATH`：

```cpp
static const char* IMAGE_PATH = "/img/demo.jpg";
```

> 找不到该路径时会自动回退：先扫描 `/img/` 目录，再扫描根目录，取第一个支持的图片文件。

3. 按需修改抖动算法：

```cpp
static const DitherMethod DITHER_METHOD = DITHER_BAYER8;
```

4. 编译上传。**务必在开发板菜单中启用 OPI PSRAM**。日志从 UART1 输出（GPIO43 TX / GPIO44 RX，115200 baud）。

## 抖动参数（编译期常量）

sketch 顶部以编译期常量暴露完整的 `DitherConfig` 参数集。除两个标注 **E1004 baseline** 的值外，初始值全部为库默认值（`src/dither/Dither.h`）。这两个值沿用本示例升级前的硬编码设置，是为 13.3 英寸大面板特意挑选的，升级不会改变渲染结果。修改后重新烧录生效；需要实时串口调参请使用 `reTerminal_E1004_SDcard_Color6_DitherCompare` 示例（命令 `g/s/d/t/e/w/p/l/m/k` 覆盖全部参数）。

```cpp
static const DitherMethod  DITHER_METHOD        = DITHER_BAYER8; // 算法（7 选 1）
static const float         DITHER_GAMMA         = 1.0f;        // x'=pow(x,1/gamma)，>1 提亮
static const bool          DITHER_SERPENTINE    = true;        // E1004 baseline：蛇形扫描（库默认 false）
static const bool          DITHER_LEGACY_CLAMP  = false;       // E1004 baseline：宽钳位[-255,510]，照片友好（库默认 true=窄[0,255]）
static const float         DITHER_SAT_BOOST     = 0.0f;        // 饱和度增强 [0,1]
static const float         DITHER_DARKNESS_BIAS = 0.0f;        // 暗度偏移 [0,0.5]
static const float         DITHER_CONTRAST      = 1.0f;        // 对比度，1.0=不变
static const float         DITHER_DIFF_STRENGTH = 1.0f;        // 误差扩散强度 [0,1]
static const float         DITHER_WARMTH        = 0.0f;        // 色温 [-1,1]，+偏暖/-偏冷
static const ColorMetric   DITHER_COLOR_METRIC  = METRIC_RGB;  // 颜色距离度量
```

7 种算法：`DITHER_NONE`（最近色）、`DITHER_BAYER8`（有序抖动）、`DITHER_FS`（Floyd-Steinberg）、`DITHER_ATKINSON`、`DITHER_BURKES`、`DITHER_SIERRA3`、`DITHER_PALETTE_MIX`（双色混合，离群色最准）。

> **默认算法为什么是 BAYER8？** 1200×1600 大面板上 BAYER8 无需误差缓冲、速度可预期，是大面板上最稳妥的选择。由于本示例直接输出打包 4bpp（见下），扩散类算法（FS 等）的内存开销也很小，可以放心尝试。

## 大面板内存策略

13.3 英寸面板像素是 7.3 英寸面板的 5 倍，内存预算完全不同：

- **直接 4bpp 输出**：`dither_image_4bpp` + 复用的 `DitherContext`，跳过 1 字节/像素的量化索引缓冲，直接产出面板格式（1200×1600 时省 960 KB）。
- **大 BMP 流式解码**：当一张未缩放的 24-bit BMP 的缓冲解码会吃满 PSRAM 时，自动改用逐行流式解码（`stream_large_bmp_if_needed`）。要求 `DISPLAY_FIT = FIT_ORIGINAL` 且原图即面板可容纳尺寸；不能缩放，需要缩放时请先在 PC 上预处理。
- 解码器分配仍可能耗尽 PSRAM——必要时用更小的源图或预处理成面板尺寸。

默认布局：`DISPLAY_FIT = FIT_ORIGINAL`、`DISPLAY_SCALE = 1.0f`、`DISPLAY_ANCHOR = ANCHOR_CENTER`（图片按原始尺寸居中，不缩放）。

## 支持的图片格式

- JPG / JPEG（baseline，8-bit YCbCr 或灰度）
- BMP（24-bit BGR 无压缩，或 4-bit indexed）
- PNG（通过 pngle 解码，RGBA 会合成到白色背景）

实际格式按魔数识别——扩展名不符时会自动纠正并打印警告。

## 自定义调色板（校准后使用）

库内置的 E6 调色板（`src/dither/Palettes.h`）已经是校准值。如果想用你自己面板的实测值覆盖，可以用色度计测量面板上 6 种颜色的实际 RGB 值，填入代码顶部的 `kCalibratedE6_Rgb` 数组：

```cpp
#define USE_CALIBRATED_PALETTE 1  // 改成 1 启用
static const Rgb kCalibratedE6_Rgb[6] = {
    {255, 255, 255},   // WHITE  -- 替换为实测值
    { 29, 185,  84},   // GREEN  -- 替换为实测值
    {229,  57,  53},   // RED    -- 替换为实测值
    {255, 216,   0},   // YELLOW -- 替换为实测值
    {  0,  76, 255},   // BLUE   -- 替换为实测值
    {  0,   0,   0},   // BLACK  -- 替换为实测值
};
```

改成 `1` 后重新烧录，抖动会使用你面板的实测颜色，混合色（肤色、天空、渐变）会更准确。

