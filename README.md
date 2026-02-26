# AI图像分类器

AI图像分类器是一个基于YOLOv11模型的Windows桌面应用程序，使用C++和MFC开发。该应用程序能够对大量图像进行自动分类，特别适用于工业视觉检测、质量控制等场景。

## 功能特性

- **高性能图像分类**：基于ONNX Runtime和YOLOv11模型，支持GPU加速
- **批量处理**：可处理整个文件夹中的图像
- **多类别分类**：支持自定义标签文件进行多类别分类
- **实时预览**：在处理过程中可预览图像
- **进度跟踪**：显示处理进度和统计信息
- **断点续传**：支持中断后从上次处理的位置继续
- **CSV结果导出**：将分类结果导出为CSV文件
- **阈值调节**：可调节置信度阈值来过滤结果
- **差分图像处理**：支持特定类别（如DG、ZT）的差分图像复制功能

## 技术架构

### 依赖库
- **OpenCV**：图像处理和计算机视觉操作
- **ONNX Runtime**：高效推理引擎，支持CPU和GPU加速
- **MFC**：Windows桌面界面开发框架

### 核心组件
- **YOLO11Classifier**：封装了ONNX Runtime推理逻辑的分类器
- **CSVManager**：管理分类结果的CSV文件读写
- **ThresholdAnalysis**：处理置信度阈值分析
- **MatchConfigDlg**：配置差分图像处理参数

## 安装要求

- Windows 10/11操作系统
- Visual Studio 2019或更高版本
- OpenCV 4.x
- ONNX Runtime（CPU或GPU版本）
- CUDA（可选，用于GPU加速）

## 编译指南

1. 克隆项目：
   ```bash
   git clone https://github.com/happybear1015/Img-av-filter-and-diff-copy.git
   ```

2. 配置项目依赖：
   - 下载并安装OpenCV
   - 下载ONNX Runtime库
   - 在Visual Studio中配置包含目录和库目录

3. 打开解决方案文件 `AIImageClassifier.sln`

4. 选择适当的配置（Debug/Release）和平台（x64）

5. 编译项目

## 使用方法

1. **加载模型**：
   - 点击"浏览模型"选择ONNX格式的模型文件
   - 确保同目录下有`labels.txt`文件定义类别名称
   - 点击"加载模型"加载模型

2. **设置路径**：
   - 选择源图像文件夹
   - 选择输出文件夹

3. **开始分类**：
   - 点击"开始筛选"按钮开始处理
   - 可随时点击"停止"按钮中断处理

4. **查看结果**：
   - 在结果列表中查看分类详情
   - 通过阈值滑块调节置信度阈值
   - 查看分类统计信息

## 高级功能

### 阈值分析
- 使用滑动条调节置信度阈值
- 实时查看符合条件的图像数量和分布

### 断点续传
- 处理中断后重新启动会跳过已完成的图像
- 基于临时文件跟踪已处理的图像

### 差分图像处理
- 当模型包含DG和ZT类别时，激活差分配置功能
- 可配置差分图像来源和保存路径

## 性能优化

- **GPU加速**：支持CUDA加速推理
- **多线程处理**：使用独立线程处理图像分类，避免UI冻结
- **内存管理**：优化内存使用，减少不必要的拷贝操作
- **批处理**：高效处理大量图像

## 文件结构

```
AIImageClassifier/
├── AIImageClassifier.sln          # Visual Studio解决方案
├── AIImageClassifier.vcxproj      # 项目配置文件
├── AIImageClassifierDlg.cpp/h     # 主对话框实现
├── YOLO11CLASS.cpp/h              # YOLOv11分类器实现
├── CSVManager.cpp/h               # CSV结果管理
├── ThresholdAnalysis.cpp/h        # 阈值分析功能
├── MatchConfigDlg.cpp/h           # 差分配置对话框
├── include/                       # 头文件
├── onnxruntime-win-x64-gpu-1.20.1 # ONNX Runtime库
├── opencv/                        # OpenCV库
└── README.md                      # 项目说明文档
```

## 配置文件

### labels.txt
定义模型的类别名称，每行一个类别名称，例如：
```
BP
DG
HL
ZT
```

## 常见问题

1. **模型加载失败**：
   - 确认ONNX模型文件完整性
   - 检查`labels.txt`文件是否存在且格式正确

2. **GPU加速未生效**：
   - 确认CUDA和cuDNN安装正确
   - 检查ONNX Runtime GPU版本是否正确安装

3. **处理速度慢**：
   - 确认是否启用了GPU加速
   - 检查系统资源使用情况

## 开发说明

本项目采用模块化设计，主要分为以下几个模块：

- **UI模块**：负责用户交互界面
- **推理模块**：负责图像分类推理
- **数据管理模块**：负责图像和结果数据管理
- **配置模块**：负责参数配置和持久化

## 贡献

欢迎提交Issue和Pull Request来改进此项目。

## 许可证

本项目采用MIT许可证。