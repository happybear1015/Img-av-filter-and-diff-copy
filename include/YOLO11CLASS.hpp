// YOLO11CLASS.hpp
#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <sstream>

struct ClassificationResult {
    int classId;
    float confidence;
    std::string className;

    ClassificationResult(int id = -1, float conf = 0.0f, const std::string& name = "")
        : classId(id), confidence(conf), className(name) {}
};

namespace utils {
    // 辅助函数：宽字符串转UTF8
    std::string WStringToUTF8(const std::wstring& wstr);

    // 加载类别名称
    std::vector<std::string> getClassNames(const std::wstring& w_path);

    // 计算向量乘积
    size_t vectorProduct(const std::vector<int64_t>& vector);

    // 图像预处理
    void preprocessImageToTensor(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& targetShape,
        const cv::Scalar& color = cv::Scalar(0, 0, 0),
        bool scaleUp = true,
        const std::string& strategy = "resize");

    // 绘制结果
    void drawClassificationResult(cv::Mat& image, const ClassificationResult& result,
        const cv::Point& position = cv::Point(10, 10),
        const cv::Scalar& textColor = cv::Scalar(0, 255, 0),
        double fontScaleMultiplier = 0.0008,
        const cv::Scalar& bgColor = cv::Scalar(0, 0, 0));
}

class YOLO11Classifier {
public:
    // 构造函数
    YOLO11Classifier(const std::string& modelPath, const std::string& labelsPath,
        bool useGPU = false);

    ~YOLO11Classifier() = default;

    // 分类主函数
    ClassificationResult classify(const cv::Mat& image);

    // 绘制结果
    void drawResult(cv::Mat& image, const ClassificationResult& result,
        const cv::Point& position = cv::Point(10, 10)) const;

    // 获取信息
    cv::Size getInputShape() const { return inputImageShape_; }
    bool isModelInputShapeDynamic() const { return isDynamicInputShape_; }

private:
    // 预处理 - 使用vector而非动态分配
    void preprocess(const cv::Mat& image, std::vector<float>& blob,
        std::vector<int64_t>& inputTensorShape);

    // 后处理
    ClassificationResult postprocess(const std::vector<Ort::Value>& outputTensors);

private:
    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::SessionOptions sessionOptions_;

    std::vector<const char*> inputNames_;
    std::vector<const char*> outputNames_;

    std::vector<Ort::AllocatedStringPtr> inputNodeNameAllocatedStrings_;
    std::vector<Ort::AllocatedStringPtr> outputNodeNameAllocatedStrings_;

    cv::Size inputImageShape_;
    std::vector<std::string> classNames_;

    int numInputNodes_ = 0;
    int numOutputNodes_ = 0;
    int numClasses_ = 0;
    bool isDynamicInputShape_ = false;
};