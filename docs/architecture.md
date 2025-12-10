# DeepX OCR - 系统架构文档

## 1. 多线程异步非阻塞调用流程

```mermaid
sequenceDiagram
    autonumber
    participant Main as Main Thread
    participant DetPool as Detection Thread Pool<br/>(4 threads)
    participant Detector as TextDetector
    participant DXRT as dxrt::InferenceEngine
    participant NPU as NPU Hardware
    participant CBThread as DXRT Callback Thread
    participant RecQueue as recQueue
    participant RecPool as Recognition Thread Pool<br/>(4 threads)

    Note over Main, RecPool: 初始化阶段
    Main->>Detector: init()
    Detector->>DXRT: RegisterCallback(internalCallback)
    Main->>Detector: setCallback(lambda)
    Main->>DetPool: Start 4 detection threads
    Main->>RecPool: Start 4 recognition threads

    Note over Main, RecPool: 运行阶段 (并发非阻塞)
    
    par Detection Thread Pool
        loop DetLoop[0]
            DetPool->>DetPool: Doc Preprocessing
            DetPool->>DetPool: Detection Preprocess
            DetPool->>Detector: runAsync(image, taskId, ...)
            Detector->>Detector: Create DetectionContext
            Detector->>DXRT: RunAsync(data, context)
            DXRT-->>DetPool: Return Immediately
        end
    and DetLoop[1]
        DetPool->>Detector: runAsync(...)
    and DetLoop[2]
        DetPool->>Detector: runAsync(...)
    and DetLoop[3]
        DetPool->>Detector: runAsync(...)
    end

    par NPU Inference
        DXRT->>NPU: Execute Model
        NPU-->>DXRT: Done
    and Recognition Thread Pool
        loop RecLoop[0..3]
            RecQueue->>RecPool: pop() (Wait for data)
        end
    end

    Note over CBThread: 回调阶段 (由 DXRT 线程触发)
    DXRT->>CBThread: Trigger Callback
    activate CBThread
    CBThread->>Detector: internalCallback(outputs, context)
    
    rect rgb(240, 248, 255)
        Note right of CBThread: Post-Processing
        Detector->>Detector: DBPostProcess (Decode Boxes)
        Detector->>Detector: Sort Boxes
    end

    Detector->>RecQueue: push(RecognitionTask)
    deactivate CBThread

    RecQueue-->>RecPool: Task Available
    
    par Recognition Parallel Processing
        RecPool->>RecPool: [Thread 0] Crop & Classify & Recognize
    and 
        RecPool->>RecPool: [Thread 1] Crop & Classify & Recognize
    and
        RecPool->>RecPool: [Thread 2] Crop & Classify & Recognize
    and
        RecPool->>RecPool: [Thread 3] Crop & Classify & Recognize
    end
```

## 2. 项目目录结构

```mermaid
graph TB
    subgraph "DeepX OCR Project"
        ROOT["OCR/"]
        
        subgraph "Source Code"
            SRC["src/"]
            SRC_COMMON["common/<br/>geometry, visualizer, logger"]
            SRC_PREPROC["preprocessing/<br/>uvdoc, image_ops"]
            SRC_DET["detection/<br/>text_detector, db_postprocess"]
            SRC_CLS["classification/<br/>text_classifier"]
            SRC_REC["recognition/<br/>text_recognizer, rec_postprocess"]
            SRC_PIPE["pipeline/<br/>ocr_pipeline"]
        end
        
        subgraph "Third Party Libraries"
            THIRDPARTY["3rd-party/"]
            TP_JSON["json/ (nlohmann)"]
            TP_CLIPPER["clipper2/"]
            TP_SPDLOG["spdlog/"]
            TP_OPENCV["opencv/"]
            TP_CONTRIB["opencv_contrib/<br/>(freetype module)"]
        end
        
        subgraph "Models"
            ENGINE["engine/model_files/"]
            MODEL_SERVER["server/<br/>det_v5_*.dxnn<br/>rec_v5_*.dxnn"]
            MODEL_MOBILE["mobile/<br/>det_mobile_*.dxnn<br/>rec_mobile_*.dxnn"]
        end
        
        subgraph "Build & Test"
            BUILD["build_Release/"]
            BUILD_BIN["bin/<br/>benchmark<br/>test_pipeline_async"]
            BUILD_ROOT["test_detector<br/>test_recognizer<br/>test_recognizer_mobile"]
        end
    end
    
    ROOT --> SRC
    ROOT --> THIRDPARTY
    ROOT --> ENGINE
    ROOT --> BUILD
    
    SRC --> SRC_COMMON
    SRC --> SRC_PREPROC
    SRC --> SRC_DET
    SRC --> SRC_CLS
    SRC --> SRC_REC
    SRC --> SRC_PIPE
    
    THIRDPARTY --> TP_JSON
    THIRDPARTY --> TP_CLIPPER
    THIRDPARTY --> TP_SPDLOG
    THIRDPARTY --> TP_OPENCV
    THIRDPARTY --> TP_CONTRIB
    
    ENGINE --> MODEL_SERVER
    ENGINE --> MODEL_MOBILE
    
    BUILD --> BUILD_BIN
    BUILD --> BUILD_ROOT
```

## 3. OCR Pipeline 数据流

```mermaid
flowchart LR
    subgraph Input
        IMG[("输入图像<br/>cv::Mat")]
    end
    
    subgraph DocPreprocessing["文档预处理 (可选)"]
        ORI["方向检测<br/>doc_ori_fixed.dxnn"]
        UVDOC["文档矫正<br/>UVDoc_pruned_p3.dxnn"]
    end
    
    subgraph Detection["文本检测"]
        DET640["det_v5_640.dxnn<br/>(小图)"]
        DET960["det_v5_960.dxnn<br/>(大图)"]
        DBPOST["DBPostProcess<br/>文本框解码"]
    end
    
    subgraph Classification["方向分类"]
        CLS["textline_ori.dxnn<br/>0°/180° 判断"]
    end
    
    subgraph Recognition["文本识别"]
        REC3["rec_v5_ratio_3.dxnn"]
        REC5["rec_v5_ratio_5.dxnn"]
        REC10["rec_v5_ratio_10.dxnn"]
        REC15["rec_v5_ratio_15.dxnn"]
        REC25["rec_v5_ratio_25.dxnn"]
        REC35["rec_v5_ratio_35.dxnn"]
        CTCDEC["CTC Decoder<br/>字符解码"]
    end
    
    subgraph Output
        RESULT[("OCR 结果<br/>boxes + texts")]
    end
    
    IMG --> ORI --> UVDOC
    UVDOC --> DET640
    UVDOC --> DET960
    DET640 --> DBPOST
    DET960 --> DBPOST
    DBPOST --> CLS
    CLS --> REC3 & REC5 & REC10 & REC15 & REC25 & REC35
    REC3 & REC5 & REC10 & REC15 & REC25 & REC35 --> CTCDEC
    CTCDEC --> RESULT
```

## 4. 模型配置

### Server 模型 (高精度)

| 模型文件 | 用途 | 输入尺寸 |
|---------|------|---------|
| `det_v5_640.dxnn` | 文本检测 (小图) | 640×640 |
| `det_v5_960.dxnn` | 文本检测 (大图) | 960×960 |
| `rec_v5_ratio_3.dxnn` | 文本识别 | 48×144 (ratio=3) |
| `rec_v5_ratio_5.dxnn` | 文本识别 | 48×240 (ratio=5) |
| `rec_v5_ratio_10.dxnn` | 文本识别 | 48×480 (ratio=10) |
| `rec_v5_ratio_15.dxnn` | 文本识别 | 48×720 (ratio=15) |
| `rec_v5_ratio_25.dxnn` | 文本识别 | 48×1200 (ratio=25) |
| `rec_v5_ratio_35.dxnn` | 文本识别 | 48×1680 (ratio=35) |
| `textline_ori.dxnn` | 文本方向分类 | 80×160 |
| `doc_ori_fixed.dxnn` | 文档方向检测 | - |
| `UVDoc_pruned_p3.dxnn` | 文档矫正 | 488×712 |

### Mobile 模型 (轻量级)

| 模型文件 | 用途 | 说明 |
|---------|------|------|
| `det_mobile_640.dxnn` | 文本检测 | 轻量版 |
| `det_mobile_960.dxnn` | 文本检测 | 轻量版 |
| `rec_mobile_ratio_*.dxnn` | 文本识别 | 6个不同宽高比 |
