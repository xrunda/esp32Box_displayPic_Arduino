# ESP32-S3-Box3 图片显示程序

这个项目用于在 ESP32-S3-Box3 设备上通过网络上传并显示图片。

## 功能特性

- 使用 LVGL 图形库显示图片
- **支持通过网络HTTP上传图片**（无需烧录到SPIFFS）
- 支持从 SPIFFS 文件系统加载 JPEG 图片（旧方式）
- 自动初始化显示和背光
- WiFi连接和HTTP服务器

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

5. 配置WiFi（使用menuconfig）：
```bash
idf.py menuconfig
```
在 "Image Display Configuration" 菜单中设置：
- WiFi SSID: 你的WiFi网络名称
- WiFi Password: 你的WiFi密码

6. 烧录固件和分区表：
```bash
idf.py flash
```

## 使用网络上传图片（推荐方式）

设备启动后会自动连接WiFi并启动HTTP服务器。你可以通过HTTP POST请求上传图片。

### 1. 获取设备IP地址

查看串口输出，设备连接WiFi后会显示IP地址，例如：
```
I (xxxx) display_image: Got IP address: 192.168.1.100
```

或者使用HTTP GET请求查询状态：
```bash
curl http://<device_ip>/status
```

### 2. 上传图片

#### 方式1: 上传图片文件

使用提供的Python脚本上传图片文件：
```bash
python upload_image_http.py <device_ip> <image_file>
```

例如：
```bash
python upload_image_http.py 192.168.1.100 mengm.jpg
```

或者使用curl命令：
```bash
curl -X POST -F "image=@mengm.jpg" http://<device_ip>/upload
```

#### 方式2: 通过URL上传图片（推荐）

发送图片URL，设备会自动从网络下载并显示：
```bash
python upload_image_url.py <device_ip> <image_url>
```

例如：
```bash
python upload_image_url.py 192.168.1.100 "https://example.com/image.jpg"
```

或者使用curl命令：
```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"url":"https://example.com/image.jpg"}' \
     http://<device_ip>/upload_url
```

也可以直接发送URL字符串（不需要JSON格式）：
```bash
curl -X POST -d "https://example.com/image.jpg" http://<device_ip>/upload_url
```

### 3. 查看设备状态

```bash
python upload_image_http.py <device_ip> --status
```

或使用curl：
```bash
curl http://<device_ip>/status
```

## 上传图片到 SPIFFS（旧方式，可选）

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

### 网络上传方式（推荐）

1. 配置WiFi SSID和密码（通过menuconfig）
2. 烧录程序到设备
3. 设备启动后会自动连接WiFi
4. 查看串口输出获取设备IP地址
5. 使用 `upload_image_http.py` 脚本上传图片
6. 图片会自动显示在屏幕上

### SPIFFS方式（旧方式）

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

## API端点

设备提供以下HTTP API端点：

- **POST /upload** - 上传图片文件（multipart/form-data或原始二进制）
- **POST /upload_url** - 发送图片URL，设备从网络下载并显示
  - 支持JSON格式：`{"url": "https://example.com/image.jpg"}`
  - 也支持纯文本URL：直接发送URL字符串
- **GET /status** - 查询设备状态和IP地址

## 注意事项

- **网络上传方式**：
  - 图片大小限制为500KB
  - 支持JPEG格式
  - 支持从URL下载图片（HTTP/HTTPS）
- **SPIFFS方式**：图片文件路径在代码中为 `S:/spiffs/mengm.jpg`，其中 `S:` 是注册的 LVGL 文件系统驱动器字母
- 确保图片文件大小不超过限制
- 如果图片无法显示，请检查串口日志以获取错误信息
- 支持的图片格式取决于 LVGL 的配置，通常支持 JPEG 格式
- 设备必须连接到与你的电脑相同的WiFi网络
- URL下载功能需要设备能够访问互联网（如果URL是公网地址）

## 故障排除

1. **WiFi连接失败**
   - 检查WiFi SSID和密码是否正确配置
   - 确认设备在WiFi信号覆盖范围内
   - 查看串口日志确认连接状态

2. **无法上传图片**
   - 确认设备已连接到WiFi并获取IP地址
   - 检查设备IP地址是否正确
   - 确认设备和电脑在同一WiFi网络
   - 检查图片文件大小是否超过500KB限制
   - 查看串口日志确认HTTP服务器是否启动

3. **图片无法显示**
   - 检查图片是否已正确上传（网络方式）或已上传到SPIFFS（旧方式）
   - 检查串口日志中的错误信息
   - 确认图片格式为JPEG
   - 确认图片尺寸适合屏幕（推荐320x240）

4. **SPIFFS 初始化失败**
   - 检查分区表是否正确配置
   - 尝试格式化 SPIFFS 分区（代码中已设置自动格式化）

5. **显示黑屏**
   - 检查背光是否已打开
   - 检查显示初始化是否成功
   - 查看串口日志确认初始化过程

## 参考

- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL 文档](https://docs.lvgl.io/)
- [ESP-BOX-3 BSP 文档](https://github.com/espressif/esp-bsp)

