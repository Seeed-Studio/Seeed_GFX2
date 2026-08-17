# reTerminal E1002 抖动算法示例（SD 卡版，新 Dither API）

本示例基于原有的 `reTerminal_E1002_SDcard_Color6` 示例改造，使用统一的 `dither/Dither.h` 模块和 `DitherConfig` API，支持 7 种已保留的抖动/量化方法、蛇形扫描和饱和度增强。

## 文件说明

- `reTerminal_E1002_SDcard_Color6_DitherEx.ino` -- 主 sketch
- `image_loader.h` / `image_loader.cpp` -- SD 卡图片解码器（JPG/BMP/PNG）
- `pngle.h` / `pngle.c` / `miniz.h` / `miniz.c` -- PNG 解码依赖

## 与旧示例的区别

1. `#include "dither.h"` 改为 `#include <dither/Dither.h>`
2. 使用 `DitherConfig` + `dither_image_ex()` 替代旧的 6 参数 `dither_image()`
3. 支持当前统一模块中的 7 种方法：NONE、BAYER8、FS、ATKINSON、BURKES、SIERRA3 和 PALETTE_MIX
4. 默认启用蛇形扫描和饱和度增强

## 使用步骤

1. 将图片拷贝到 SD 卡 `/img/` 目录下。
2. 修改 sketch 顶部的 `IMAGE_PATH`：

```cpp
static const char* IMAGE_PATH = "/img/demo.jpg";
```

3. 修改抖动算法：

```cpp
static const DitherMethod DITHER_METHOD = DITHER_FS;
```

4. 编译上传。

## 可调参数

```cpp
static DitherConfig ditherConfig() {
    DitherConfig cfg;
    cfg.method          = DITHER_METHOD;   // 算法
    cfg.palette         = PAL_E6;          // 6 色调色板
    cfg.gamma           = 1.1f;            // x'=pow(x,1/gamma)，>1 提亮
    cfg.serpentine      = true;            // 蛇形扫描
    cfg.saturationBoost = 0.2f;          // 饱和度增强
    cfg.darknessBias    = 0.0f;          // 暗度偏移（0=关闭，照片可调）
    return cfg;
}
```

## 布局参数

```cpp
static const DisplayAnchor DISPLAY_ANCHOR = ANCHOR_CENTER;  // 图片在面板上的锚点
static const DisplayFit    DISPLAY_FIT    = FIT_SCALE;      // 适配模式
static const float         DISPLAY_SCALE  = 0.7f;           // 缩放比例
```

- `FIT_ORIGINAL`：保持原图尺寸
- `FIT_CONTAIN`：按比例缩放至面板内
- `FIT_SCALE`：按 `DISPLAY_SCALE` 缩放

## 支持的图片格式

- JPG / JPEG（baseline，8-bit YCbCr 或灰度）
- BMP（24-bit BGR 无压缩，或 4-bit indexed）
- PNG（通过 pngle 解码，RGBA 会合成到白色背景）

## 自定义调色板（校准后使用）

如果你用色度计测量了面板上 6 种颜色的实际 RGB 值，可以填入代码顶部的 `kCalibratedE6_Rgb` 数组：

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
