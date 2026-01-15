# DeepX OCR Server

基于 Crow 框架的高性能 OCR HTTP 服务，支持并发请求处理。

## 编译

```bash
cd /home/deepx/Desktop/ocr_demo
bash build.sh
```

## 启动

```bash
./build_Release/bin/ocr_server [选项]
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-p, --port` | 服务端口 | 8080 |
| `-t, --threads` | HTTP 线程数 | 4 |
| `-v, --vis-dir` | 可视化输出目录 | output/vis |
| `-h, --help` | 帮助 | - |

## API

### POST /ocr

OCR 识别接口，支持 Base64 编码图像和 URL 两种输入方式。

**请求头**

```
Content-Type: application/json
Authorization: token <任意字符串>
```

**请求体**

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| file | string | 是 | - | Base64 编码的图像或图像 URL |
| fileType | int | 否 | 1 | 文件类型：1=图像，0=PDF（暂不支持） |
| useDocOrientationClassify | bool | 否 | false | 启用文档方向分类 |
| useDocUnwarping | bool | 否 | false | 启用文档扭曲矫正 |
| useTextlineOrientation | bool | 否 | false | 启用文本行方向矫正 |
| textDetThresh | float | 否 | 0.3 | 检测像素阈值 |
| textDetBoxThresh | float | 否 | 0.6 | 检测框阈值 |
| textDetUnclipRatio | float | 否 | 1.5 | 检测框扩张系数 |
| textRecScoreThresh | float | 否 | 0.0 | 识别置信度阈值 |
| visualize | bool | 否 | false | 生成可视化结果图像 |

**响应示例**

```json
{
    "errorCode": 0,
    "errorMsg": "success",
    "result": {
        "ocrResults": [
            {
                "prunedResult": "识别的文字",
                "score": 0.98,
                "ocrBox": [[x1,y1], [x2,y2], [x3,y3], [x4,y4]]
            }
        ],
        "ocrImage": "/static/vis/ocr_vis_xxx.jpg"
    }
}
```

**错误码**

| errorCode | 说明 |
|-----------|------|
| 0 | 成功 |
| 1001 | 参数错误 |
| 1002 | 文件处理错误 |
| 2001 | 服务内部错误 |
| 3001 | 认证失败 |

### GET /health

健康检查接口。

```json
{
    "status": "healthy",
    "service": "DeepX OCR Server",
    "version": "1.0.0"
}
```

### GET /static/vis/\<filename\>

访问可视化结果图像。

## 基准测试

使用 `benchmark/run.sh` 进行性能测试：

```bash
cd server/benchmark

# 串行模式测试（默认）
./run.sh

# 并发模式测试（4 并发）
./run.sh -c 4

# 指定运行次数
./run.sh -r 3

# 查看帮助
./run.sh -h
```

测试结果会输出到：
- `api_benchmark_results.json` - JSON 格式详细结果
- `API_benchmark_report.md` - Markdown 格式报告

## 单元测试

```bash
cd build_Release
ctest --output-on-failure
```

## 目录结构

```
server/
├── server_main.cpp      # 服务入口
├── ocr_handler.cpp/h    # OCR 请求处理器
├── file_handler.cpp/h   # 文件处理（Base64/URL）
├── json_response.cpp/h  # JSON 响应构建器
├── benchmark/           # 基准测试工具
│   ├── run.sh          # 测试脚本
│   └── images/         # 测试图片
└── tests/              # 单元测试
```

## 输出

- 可视化图像: `output/vis/`
- 日志: `logs/deepx_ocr.log`
