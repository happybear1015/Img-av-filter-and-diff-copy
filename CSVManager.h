// CSVManager.h
#pragma once
#include "pch.h"
#include <fstream>
#include <map>
#include <vector>
#include <string>

class CCSVManager
{
public:
    CCSVManager();
    virtual ~CCSVManager();

    // CSV文件管理
    bool InitializeCSVFile(const CString& outputPath);
    void SaveResultToCSV(const CString& fileName, const CString& className,
        double confidence, long long processTime, const CString& outputPath);
    void CloseCSVFile();
    bool LoadResultsFromCSV(std::map<std::string, std::vector<std::pair<std::string, float>>>& results);
    bool IsFileRecordedInCSV(const CString& fileName);

    // 获取CSV文件信息
    CString GetCSVFilePath() const { return m_csvFilePath; }
    int GetProcessedCount() const { return m_processedCount; }
    bool IsCSVFileOpen() const { return m_csvFile.is_open(); }

private:
    // 文件操作
    std::ofstream m_csvFile;
    CString m_csvFilePath;
    int m_processedCount;

    // 辅助函数
    bool ParseCSVLine(const std::wstring& w_line, CString& fileName, CString& className, float& confidence);
    bool CreateDirectoryRecursive(const CString& path);
    bool PathExists(const CString& path);

    // 字符编码转换
    std::string CT2UTF8(const CString& str);
    CString UTF82CT(const std::string& utf8Str);
    std::wstring CStringToWString(const CString& str);
    CString WStringToCString(const std::wstring& wstr);

    // 路径清理
    CString CleanPath(const CString& path);
};