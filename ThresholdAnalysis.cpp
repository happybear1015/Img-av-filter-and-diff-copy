// ThresholdAnalysis.cpp
#include "pch.h"
#include "ThresholdAnalysis.h"

CThresholdAnalysis::CThresholdAnalysis()
    : m_currentThreshold(0.5f)
    , m_totalImages(0)
    , m_validImages(0)
{
}

CThresholdAnalysis::~CThresholdAnalysis()
{
}

void CThresholdAnalysis::SetClassificationResults(const std::map<std::string, std::vector<std::pair<std::string, float>>>& results)
{
    m_originalResults = results;

    // 计算总图像数量
    m_totalImages = 0;
    for (const auto& pair : m_originalResults) {
        m_totalImages += pair.second.size();
    }

    // 初始应用当前阈值
    ApplyThreshold(m_currentThreshold);
}

void CThresholdAnalysis::ApplyThreshold(float threshold)
{
    m_currentThreshold = threshold;
    m_filteredResults.clear();
    m_distribution.clear();
    m_validImages = 0;

    for (const auto& categoryPair : m_originalResults) {
        const std::string& categoryName = categoryPair.first;
        std::vector<std::pair<std::string, float>> validImages;

        for (const auto& imagePair : categoryPair.second) {
            if (imagePair.second >= threshold) {
                validImages.push_back(imagePair);
                m_validImages++;
            }
        }

        if (!validImages.empty()) {
            m_filteredResults[categoryName] = validImages;
            m_distribution[categoryName] = validImages.size();
        }
    }
}

std::map<std::string, int> CThresholdAnalysis::GetDistribution() const
{
    return m_distribution;
}

int CThresholdAnalysis::GetValidImageCount() const
{
    return m_validImages;
}

int CThresholdAnalysis::GetTotalImageCount() const
{
    return m_totalImages;
}

std::map<std::string, std::vector<std::pair<std::string, float>>> CThresholdAnalysis::GetFilteredResults() const
{
    return m_filteredResults;
}

float CThresholdAnalysis::GetCurrentThreshold() const
{
    return m_currentThreshold;
}