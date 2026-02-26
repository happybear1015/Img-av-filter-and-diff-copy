// CSVExporter.h
#pragma once
#include <fstream>
#include <string>

class CString;

struct ClassificationResult {
    CString fileName;
    CString className;
    float confidence;
    double processingTime;
};

class CSVExporter {
public:
    CSVExporter();
    ~CSVExporter();

    bool Initialize(const CString& outputPath, const CString& fileName);
    void AddResult(const ClassificationResult& result);
    void Close();
    bool IsInitialized() const;

private:
    std::ofstream m_fileStream;
    bool m_bInitialized;
};