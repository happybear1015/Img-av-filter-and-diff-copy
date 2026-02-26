// ThresholdAnalysis.h
#pragma once
#include <map>
#include <vector>
#include <string>

class CThresholdAnalysis
{
public:
    CThresholdAnalysis();
    virtual ~CThresholdAnalysis();

    // 设置原始分类结果
    void SetClassificationResults(const std::map<std::string, std::vector<std::pair<std::string, float>>>& results);

    // 应用阈值过滤
    void ApplyThreshold(float threshold);

    // 获取分布统计
    std::map<std::string, int> GetDistribution() const;

    // 获取符合阈值的图像数量
    int GetValidImageCount() const;

    // 获取总图像数量
    int GetTotalImageCount() const;

    // 获取过滤后的结果
    std::map<std::string, std::vector<std::pair<std::string, float>>> GetFilteredResults() const;

    // 获取当前阈值
    float GetCurrentThreshold() const;

private:
    std::map<std::string, std::vector<std::pair<std::string, float>>> m_originalResults;
    std::map<std::string, std::vector<std::pair<std::string, float>>> m_filteredResults;
    std::map<std::string, int> m_distribution;
    float m_currentThreshold;
    int m_totalImages;
    int m_validImages;
};