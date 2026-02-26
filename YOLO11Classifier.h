// YOLO11Classifier.h
#pragma once
#include "ImageClassifier.h"
#include <opencv2/opencv.hpp>

class YOLO11Classifier : public ImageClassifier {
public:
    YOLO11Classifier(const std::string& modelPath, const std::string& labelsPath, bool useGPU);
    virtual ~YOLO11Classifier();

    bool Initialize(const std::string& modelPath, const std::string& labelsPath) override;
    std::string ClassifyImage(const std::string& imagePath, float& confidence) override;
    bool IsInitialized() const override;
    std::vector<std::string> GetClassNames() const override;

private:
    cv::dnn::Net m_net;
    std::vector<std::string> m_classNames;
    bool m_bInitialized;

    cv::Mat PreprocessImage(const cv::Mat& image);
    std::string PostprocessResults(const cv::Mat& outputs);
};