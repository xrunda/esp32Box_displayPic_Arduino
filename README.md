# ESP32-S3-Box3 图片显示程序

这个项目用于在 ESP32-S3-Box3 设备上显示图片。

## 功能特性

- 使用 LVGL 图形库显示图片
- 支持从 SPIFFS 文件系统加载 JPEG 图片
- 自动初始化显示和背光

## 硬件要求

- ESP32-S3-Box3 开发板

## 编译和烧录

1. 确保已安装 ESP-IDF (版本 >= 5.0)

2. 设置 ESP-IDF 环境变量（根据你的环境变量位置文件）：
```bash
cd /Users/liguang/Documents/xRunda/project/AI/github/esp-idf
. ./export.sh
cd /Users/liguang/Documents/xRunda/project/AI/github/esp32Box_displayPic_Arduino
```

3. 设置目标芯片（如果还没有设置）：
```bash
idf.py set-target esp32s3
```

4. 编译项目：
```bash
idf.py build
```

5. 烧录固件和分区表：
```bash
idf.py flash
```

## 上传图片到 SPIFFS

在显示图片之前，需要将 `mengm.jpg` 上传到 SPIFFS 分区。

### 方法1: 使用 mkspiffs 工具

1. 安装 mkspiffs 工具（如果还没有安装）

2. 创建 SPIFFS 镜像：
```bash
mkspiffs -c spiffs_image -b 4096 -p 256 -s 0x100000 mengm.jpg
```

3. 烧录 SPIFFS 镜像：
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x110000 spiffs_image
```

### 方法2: 使用 ESP-IDF 的 spiffsgen.py

1. 创建 spiffs 目录并复制图片：
```bash
mkdir -p spiffs
cp mengm.jpg spiffs/
```

2. 生成并烧录 SPIFFS 镜像：
```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x100000 spiffs spiffs_image
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x110000 spiffs_image
```

### 方法3: 使用 idf.py 和 spiffsgen.py（推荐）

1. 创建 spiffs 目录并复制图片：
```bash
mkdir -p spiffs
cp mengm.jpg spiffs/
```

2. 在 CMakeLists.txt 中添加 SPIFFS 镜像生成（如果需要）

3. 使用 idf.py 烧录：
```bash
idf.py spiffsgen
idf.py spiffs
```

## 使用说明

1. 确保图片文件 `mengm.jpg` 已上传到 SPIFFS 分区
2. 烧录程序到设备
3. 重启设备，图片应该会显示在屏幕上

## 项目结构

```
esp32Box_displayPic_Arduino/
├── CMakeLists.txt          # 项目主 CMakeLists.txt
├── partitions.csv          # 分区表（包含 SPIFFS 分区）
├── main/
│   ├── CMakeLists.txt      # 主组件 CMakeLists.txt
│   ├── idf_component.yml   # 组件依赖配置
│   └── display_image.c     # 主程序文件
├── mengm.jpg               # 源图片文件（需要上传到 SPIFFS）
└── README.md               # 本文件
```

## 注意事项

- 图片文件路径在代码中为 `S:/spiffs/mengm.jpg`，其中 `S:` 是注册的 LVGL 文件系统驱动器字母
- 确保图片文件大小不超过 SPIFFS 分区大小（当前设置为 1MB）
- 如果图片无法显示，请检查串口日志以获取错误信息
- 支持的图片格式取决于 LVGL 的配置，通常支持 JPEG 格式

## 故障排除

1. **图片无法显示**
   - 检查图片是否已正确上传到 SPIFFS
   - 检查串口日志中的错误信息
   - 确认图片格式为 JPEG

2. **SPIFFS 初始化失败**
   - 检查分区表是否正确配置
   - 尝试格式化 SPIFFS 分区（代码中已设置自动格式化）

3. **显示黑屏**
   - 检查背光是否已打开
   - 检查显示初始化是否成功
   - 查看串口日志确认初始化过程

## 参考

- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL 文档](https://docs.lvgl.io/)
- [ESP-BOX-3 BSP 文档](https://github.com/espressif/esp-bsp)

