// YOLO11CLASS.cpp
#include "pch.h"
#include "YOLO11CLASS.hpp"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <Windows.h>

namespace utils {
    // 使用Windows API进行编码转换
    std::string WStringToUTF8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len == 0) return std::string();

        std::vector<char> buffer(utf8Len);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.data(), utf8Len, nullptr, nullptr);

        return std::string(buffer.data(), buffer.size() - 1);
    }

    std::vector<std::string> getClassNames(const std::wstring& w_path) {
        std::vector<std::string> classNames;
        std::wifstream infile(w_path);

        if (infile) {
            std::wstring w_line;
            while (std::getline(infile, w_line)) {
                if (!w_line.empty()) {
                    if (w_line.back() == L'\r') {
                        w_line.pop_back();
                    }

                    // 移除BOM标记
                    if (!w_line.empty() && w_line[0] == 0xFEFF) {
                        w_line = w_line.substr(1);
                    }

                    // 转换编码
                    std::string utf8_line = WStringToUTF8(w_line);
                    classNames.emplace_back(utf8_line);
                }
            }
        }
        return classNames;
    }

    size_t vectorProduct(const std::vector<int64_t>& vector) {
        if (vector.empty()) return 0;
        return std::accumulate(vector.begin(), vector.end(), 1LL, std::multiplies<int64_t>());
    }

    void preprocessImageToTensor(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& targetShape,
        const cv::Scalar& color,
        bool scaleUp,
        const std::string& strategy) {
        if (image.empty()) return;

        if (strategy == "letterbox") {
            float r = std::min(static_cast<float>(targetShape.height) / image.rows,
                static_cast<float>(targetShape.width) / image.cols);
            if (!scaleUp) {
                r = std::min(r, 1.0f);
            }
            int newUnpadW = static_cast<int>(std::round(image.cols * r));
            int newUnpadH = static_cast<int>(std::round(image.rows * r));

            cv::Mat resizedTemp;
            cv::resize(image, resizedTemp, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);

            int dw = targetShape.width - newUnpadW;
            int dh = targetShape.height - newUnpadH;

            cv::copyMakeBorder(resizedTemp, outImage, dh / 2, dh - dh / 2, dw / 2, dw - dw / 2,
                cv::BORDER_CONSTANT, color);
        }
        else {
            cv::resize(image, outImage, targetShape, 0, 0, cv::INTER_LINEAR);
        }
    }

    void drawClassificationResult(cv::Mat& image, const ClassificationResult& result,
        const cv::Point& position,
        const cv::Scalar& textColor,
        double fontScaleMultiplier,
        const cv::Scalar& bgColor) {
        if (image.empty() || result.classId == -1) return;

        std::ostringstream ss;
        ss << result.className << ": " << std::fixed << std::setprecision(2) << result.confidence * 100 << "%";
        std::string text = ss.str();

        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = std::min(image.rows, image.cols) * fontScaleMultiplier;
        if (fontScale < 0.4) fontScale = 0.4;
        int thickness = std::max(1, static_cast<int>(fontScale * 1.8));
        int baseline = 0;

        cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
        baseline += thickness;

        cv::Point textPosition = position;
        if (textPosition.x < 0) textPosition.x = 0;
        if (textPosition.y < textSize.height) textPosition.y = textSize.height + 2;

        cv::rectangle(image,
            cv::Point(textPosition.x, textPosition.y - textSize.height - baseline / 3),
            cv::Point(textPosition.x + textSize.width, textPosition.y + baseline / 2),
            bgColor, cv::FILLED);
        cv::putText(image, text, cv::Point(textPosition.x, textPosition.y),
            fontFace, fontScale, textColor, thickness, cv::LINE_AA);
    }
}

// 构造函数 - 关键优化
YOLO11Classifier::YOLO11Classifier(const std::string& modelPath, const std::string& labelsPath, bool useGPU)
    : env_(ORT_LOGGING_LEVEL_WARNING, "ONNX_CLASSIFICATION_ENV")
    , session_(nullptr)
{
    // 优化会话选项
    sessionOptions_.SetIntraOpNumThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
    sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    sessionOptions_.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL); // 顺序执行，更稳定

    // GPU配置 - 使用您GPU版本中的方式
    std::vector<std::string> availableProviders = Ort::GetAvailableProviders();
    auto cudaAvailable = std::find(availableProviders.begin(), availableProviders.end(), "CUDAExecutionProvider");

    if (useGPU && cudaAvailable != availableProviders.end()) {
        try {
            OrtCUDAProviderOptions cudaOption;
            cudaOption.device_id = 0;
            cudaOption.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchDefault;
            cudaOption.gpu_mem_limit = SIZE_MAX;
            cudaOption.arena_extend_strategy = 0;
            cudaOption.do_copy_in_default_stream = 1;
            cudaOption.has_user_compute_stream = 0;

            sessionOptions_.AppendExecutionProvider_CUDA(cudaOption);
            std::cout << "[INFO] CUDA GPU加速已启用" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[WARN] CUDA初始化失败: " << e.what() << "，回退到CPU" << std::endl;
            useGPU = false;
        }
    }

    // 模型路径转换（Windows）
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> w_modelPathBuffer(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, w_modelPathBuffer.data(), wideLen);
    std::wstring w_modelPath(w_modelPathBuffer.data());

    int labelsWideLen = MultiByteToWideChar(CP_UTF8, 0, labelsPath.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> w_labelsPathBuffer(labelsWideLen);
    MultiByteToWideChar(CP_UTF8, 0, labelsPath.c_str(), -1, w_labelsPathBuffer.data(), labelsWideLen);
    std::wstring w_labelsPath(w_labelsPathBuffer.data());
#else
    // Linux平台处理
    std::wstring w_modelPath, w_labelsPath;
#endif

    try {
        // 创建会话
#ifdef _WIN32
        session_ = std::make_unique<Ort::Session>(env_, w_modelPath.c_str(), sessionOptions_);
#else
        session_ = std::make_unique<Ort::Session>(env_, modelPath.c_str(), sessionOptions_);
#endif
    }
    catch (const Ort::Exception& e) {
        throw std::runtime_error("无法加载ONNX模型: " + std::string(e.what()));
    }

    Ort::AllocatorWithDefaultOptions allocator;

    // 获取输入输出信息
    numInputNodes_ = session_->GetInputCount();
    numOutputNodes_ = session_->GetOutputCount();

    if (numInputNodes_ == 0) throw std::runtime_error("模型没有输入节点");
    if (numOutputNodes_ == 0) throw std::runtime_error("模型没有输出节点");

    // 输入节点
    auto input_node_name = session_->GetInputNameAllocated(0, allocator);
    inputNodeNameAllocatedStrings_.push_back(std::move(input_node_name));
    inputNames_.push_back(inputNodeNameAllocatedStrings_.back().get());

    // 获取输入形状
    Ort::TypeInfo inputTypeInfo = session_->GetInputTypeInfo(0);
    auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> modelInputTensorShapeVec = inputTensorInfo.GetShape();

    // 确定输入尺寸
    if (modelInputTensorShapeVec.size() == 4) {
        isDynamicInputShape_ = (modelInputTensorShapeVec[2] == -1 || modelInputTensorShapeVec[3] == -1);

        if (!isDynamicInputShape_) {
            inputImageShape_ = cv::Size(
                static_cast<int>(modelInputTensorShapeVec[3]),
                static_cast<int>(modelInputTensorShapeVec[2])
            );
        }
        else {
            // 动态输入，使用常见尺寸
            inputImageShape_ = cv::Size(224, 224); // 分类模型常用尺寸
        }
    }
    else {
        inputImageShape_ = cv::Size(224, 224);
        isDynamicInputShape_ = true;
    }

    // 输出节点
    auto output_node_name = session_->GetOutputNameAllocated(0, allocator);
    outputNodeNameAllocatedStrings_.push_back(std::move(output_node_name));
    outputNames_.push_back(outputNodeNameAllocatedStrings_.back().get());

    // 获取类别数
    Ort::TypeInfo outputTypeInfo = session_->GetOutputTypeInfo(0);
    auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outputTensorShapeVec = outputTensorInfo.GetShape();

    if (!outputTensorShapeVec.empty()) {
        // 通常输出形状是 [1, num_classes] 或 [num_classes]
        if (outputTensorShapeVec.size() == 2) {
            numClasses_ = static_cast<int>(outputTensorShapeVec[1]);
        }
        else if (outputTensorShapeVec.size() == 1) {
            numClasses_ = static_cast<int>(outputTensorShapeVec[0]);
        }
        else {
            // 查找最大维度作为类别数
            for (auto dim : outputTensorShapeVec) {
                if (dim > numClasses_) numClasses_ = static_cast<int>(dim);
            }
        }
    }

    // 加载类别名称
    classNames_ = utils::getClassNames(w_labelsPath);

    std::cout << "[INFO] 模型加载成功" << std::endl;
    std::cout << "  - 输入尺寸: " << inputImageShape_.width << "x" << inputImageShape_.height << std::endl;
    std::cout << "  - 类别数: " << numClasses_ << std::endl;
    std::cout << "  - 使用GPU: " << (useGPU ? "是" : "否") << std::endl;
}

// 优化的预处理函数 - 使用vector避免动态分配
void YOLO11Classifier::preprocess(const cv::Mat& image, std::vector<float>& blob,
    std::vector<int64_t>& inputTensorShape) {
    if (image.empty()) {
        throw std::runtime_error("输入图像为空");
    }

    // 1. 调整尺寸
    cv::Mat resizedImage;
    utils::preprocessImageToTensor(image, resizedImage, inputImageShape_,
        cv::Scalar(0, 0, 0), true, "resize");

    // 2. BGR转RGB
    cv::Mat rgbImage;
    cv::cvtColor(resizedImage, rgbImage, cv::COLOR_BGR2RGB);

    // 3. 转换为float并归一化
    cv::Mat floatImage;
    rgbImage.convertTo(floatImage, CV_32FC3, 1.0 / 255.0);

    // 4. 设置张量形状 [1, C, H, W]
    inputTensorShape = {
        1, 3,
        static_cast<int64_t>(inputImageShape_.height),
        static_cast<int64_t>(inputImageShape_.width)
    };

    size_t tensorSize = utils::vectorProduct(inputTensorShape);
    blob.resize(tensorSize);

    // 5. HWC转CHW - 优化循环
    int channels = 3;
    int height = inputImageShape_.height;
    int width = inputImageShape_.width;

    // 使用指针访问提高速度
    float* blobPtr = blob.data();

    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            const cv::Vec3f* row = floatImage.ptr<cv::Vec3f>(h);
            for (int w = 0; w < width; ++w) {
                blobPtr[c * height * width + h * width + w] = row[w][c];
            }
        }
    }
}

// 优化的后处理函数
ClassificationResult YOLO11Classifier::postprocess(const std::vector<Ort::Value>& outputTensors) {
    if (outputTensors.empty() || outputTensors[0].GetTensorData<float>() == nullptr) {
        return ClassificationResult(-1, 0.0f, "无输出");
    }

    const float* rawOutput = outputTensors[0].GetTensorData<float>();
    const std::vector<int64_t> outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

    // 计算输出元素数量
    size_t outputSize = 1;
    for (auto dim : outputShape) {
        if (dim > 0) outputSize *= dim;
    }

    if (outputSize == 0) {
        return ClassificationResult(-1, 0.0f, "输出为空");
    }

    // 找到最大值的索引
    int bestClassId = 0;
    float maxScore = rawOutput[0];

    // 如果输出是二维的 [batch, classes]，取第一个batch
    size_t startIdx = 0;
    if (outputShape.size() == 2 && outputShape[0] > 1) {
        // 有多个batch，取第一个
        startIdx = 0;
    }

    // 寻找最高分数
    for (size_t i = startIdx + 1; i < outputSize; i++) {
        if (rawOutput[i] > maxScore) {
            maxScore = rawOutput[i];
            bestClassId = static_cast<int>(i - startIdx);
        }
    }

    // 获取类别名称
    std::string className;
    if (!classNames_.empty() && bestClassId >= 0 && bestClassId < static_cast<int>(classNames_.size())) {
        className = classNames_[bestClassId];
    }
    else {
        className = "类别_" + std::to_string(bestClassId);
    }

    return ClassificationResult(bestClassId, maxScore, className);
}

// 分类函数
ClassificationResult YOLO11Classifier::classify(const cv::Mat& image) {
    if (image.empty()) {
        return ClassificationResult(-1, 0.0f, "图像为空");
    }

    std::vector<float> blob;
    std::vector<int64_t> inputTensorShape;

    try {
        preprocess(image, blob, inputTensorShape);
    }
    catch (const std::exception& e) {
        std::cerr << "预处理失败: " << e.what() << std::endl;
        return ClassificationResult(-1, 0.0f, "预处理错误");
    }

    if (blob.empty()) {
        return ClassificationResult(-1, 0.0f, "预处理无数据");
    }

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        blob.data(),
        blob.size(),
        inputTensorShape.data(),
        inputTensorShape.size()
        );

    std::vector<Ort::Value> outputTensors;
    try {
        outputTensors = session_->Run(
            Ort::RunOptions{ nullptr },
            inputNames_.data(),
            &inputTensor,
            numInputNodes_,
            outputNames_.data(),
            numOutputNodes_
        );
    }
    catch (const Ort::Exception& e) {
        std::cerr << "ONNX推理失败: " << e.what() << std::endl;
        return ClassificationResult(-1, 0.0f, "推理错误");
    }

    try {
        return postprocess(outputTensors);
    }
    catch (const std::exception& e) {
        std::cerr << "后处理失败: " << e.what() << std::endl;
        return ClassificationResult(-1, 0.0f, "后处理错误");
    }
}

void YOLO11Classifier::drawResult(cv::Mat& image, const ClassificationResult& result,
    const cv::Point& position) const {
    utils::drawClassificationResult(image, result, position, cv::Scalar(0, 255, 0), 0.0008, cv::Scalar(0, 0, 0));
}