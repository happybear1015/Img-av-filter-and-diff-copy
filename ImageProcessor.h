// ImageProcessor.h
#pragma once
#include <opencv2/opencv.hpp>

class ImageProcessor {
public:
    static cv::Mat LoadImage(const CString& imagePath);
    static cv::Mat ResizeImageForPreview(const cv::Mat& image, const CSize& targetSize);
    static HBITMAP MatToHBITMAP(const cv::Mat& image);
    static bool SaveImage(const cv::Mat& image, const CString& outputPath);
};