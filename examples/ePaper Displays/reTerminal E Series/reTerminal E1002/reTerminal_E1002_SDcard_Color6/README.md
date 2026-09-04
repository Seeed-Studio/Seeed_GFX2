# reTerminal E1002 SD 卡图片 6 色显示（新 Dither API）

将 SD 卡上的 JPG/BMP/PNG 图片显示到 reTerminal E1002 的 7.3 英寸 6 色电子墨水屏（800×480，ED2208）。使用统一的 `dither/Dither.h` 模块和 `DitherConfig` API，以编译期常量暴露完整的抖动参数集（初始值全部为库默认值），支持 7 种抖动/量化方法。图片自动缩放到 800×480 全屏显示。

## 文件说明

- `reTerminal_E1002_SDcard_Color6.ino` -- 主 sketch
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
static const DitherMethod DITHER_METHOD = DITHER_FS;
```

4. 编译上传。日志从 UART1 输出（GPIO43 TX / GPIO44 RX，115200 baud）。

## 抖动参数（编译期常量）

sketch 顶部以编译期常量暴露完整的 `DitherConfig` 参数集，初始值全部为库默认值（`src/dither/Dither.h`）。修改后重新烧录生效；需要实时串口调参请使用 `reTerminal_E1002_SDcard_Color6_DitherCompare` 示例（命令 `g/s/d/t/e/w/p/l/m/k` 覆盖全部参数）。

```cpp
static const DitherMethod  DITHER_METHOD        = DITHER_FS;   // 算法（7 选 1）
static const float         DITHER_GAMMA         = 1.0f;        // x'=pow(x,1/gamma)，>1 提亮
static const bool          DITHER_SERPENTINE    = false;       // 蛇形扫描
static const bool          DITHER_LEGACY_CLAMP  = true;        // true=窄钳位[0,255]（默认）；false=宽[-255,510]
static const float         DITHER_SAT_BOOST     = 0.0f;        // 饱和度增强 [0,1]
static const float         DITHER_DARKNESS_BIAS = 0.0f;        // 暗度偏移 [0,0.5]
static const float         DITHER_CONTRAST      = 1.0f;        // 对比度，1.0=不变
static const float         DITHER_DIFF_STRENGTH = 1.0f;        // 误差扩散强度 [0,1]
static const float         DITHER_WARMTH        = 0.0f;        // 色温 [-1,1]，+偏暖/-偏冷
static const ColorMetric   DITHER_COLOR_METRIC  = METRIC_RGB;  // 颜色距离度量
```

7 种算法：`DITHER_NONE`（最近色）、`DITHER_BAYER8`（有序抖动）、`DITHER_FS`（Floyd-Steinberg）、`DITHER_ATKINSON`、`DITHER_BURKES`、`DITHER_SIERRA3`、`DITHER_PALETTE_MIX`（双色混合，离群色最准）。

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


