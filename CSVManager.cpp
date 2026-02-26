// CSVManager.cpp
#include "pch.h"
#include "CSVManager.h"
#include <filesystem>
#include <shlobj.h>
#include <algorithm>

namespace fs = std::filesystem;

CCSVManager::CCSVManager()
    : m_processedCount(0)
{
}

CCSVManager::~CCSVManager()
{
    CloseCSVFile();
}

bool CCSVManager::InitializeCSVFile(const CString& outputPath)
{
    if (outputPath.IsEmpty()) {
        return false;
    }

    // 清理路径
    CString cleanOutputPath = CleanPath(outputPath);
    if (cleanOutputPath.IsEmpty()) {
        return false;
    }

    // 确保输出目录存在
    if (!PathExists(cleanOutputPath)) {
        if (!CreateDirectoryRecursive(cleanOutputPath)) {
            return false;
        }
    }

    // 检查目录写入权限
    CString testFile = cleanOutputPath + _T("\\write_test.tmp");
    HANDLE hFile = CreateFile(testFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(hFile);
    DeleteFile(testFile);

    // 使用固定的CSV文件名
    m_csvFilePath = cleanOutputPath + _T("\\classification_results.csv");

    try {
        std::wstring w_csvFilePath = CStringToWString(m_csvFilePath);

        // 确保文件目录存在
        std::wstring w_dir = w_csvFilePath.substr(0, w_csvFilePath.find_last_of(L'\\'));
        if (!fs::exists(w_dir)) {
            if (!fs::create_directories(w_dir)) {
                return false;
            }
        }

        // 检查文件是否已存在
        bool fileExists = fs::exists(w_csvFilePath);

        // 以追加模式打开文件
        m_csvFile.open(w_csvFilePath, std::ios::out | std::ios::binary | std::ios::app);
        if (!m_csvFile.is_open()) {
            return false;
        }

        // 如果文件不存在或者是新创建的，写入UTF-8 BOM和表头
        if (!fileExists || fs::file_size(w_csvFilePath) == 0) {
            // 写入UTF-8 BOM
            unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            m_csvFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));

            // 写入表头
            CString headerStr = _T("编号,文件名,类别,置信度(%),处理耗时(ms),保存路径,处理时间\r\n");
            std::string utf8Header = CT2UTF8(headerStr);
            m_csvFile.write(utf8Header.c_str(), utf8Header.length());
            m_processedCount = 0;
        }
        else {
            // 文件已存在，正确计算当前行数（排除表头）
            std::wifstream countFile(w_csvFilePath);
            if (countFile) {
                std::wstring line;
                int lineCount = 0;

                // 跳过可能的BOM
                wchar_t firstChar = countFile.get();
                if (firstChar != 0xFEFF) {
                    countFile.putback(firstChar);
                }

                while (std::getline(countFile, line)) {
                    lineCount++;
                }
                // 减去表头行，得到实际数据行数
                m_processedCount = max(0, lineCount - 1);
            }
            else {
                m_processedCount = 0;
            }
            countFile.close();
        }

        m_csvFile.flush();
        return true;

    }
    catch (const std::exception&) {
        if (m_csvFile.is_open()) {
            m_csvFile.close();
        }
        return false;
    }
    catch (...) {
        if (m_csvFile.is_open()) {
            m_csvFile.close();
        }
        return false;
    }
}

void CCSVManager::SaveResultToCSV(const CString& fileName, const CString& className,
    double confidence, long long processTime, const CString& outputPath)
{
    if (!m_csvFile.is_open()) {
        return;
    }

    try {
        // 检查是否已记录
        if (IsFileRecordedInCSV(fileName)) {
            return; // 已存在，不重复写入
        }

        CString savePath = outputPath + _T("\\") + className + _T("\\") + fileName;

        m_processedCount++;

        // 获取当前时间
        CTime currentTime = CTime::GetCurrentTime();
        CString timeStamp = currentTime.Format(_T("%Y-%m-%d %H:%M:%S"));

        // 构建CSV行（注意处理可能包含逗号的文件名）
        CString csvLine;
        csvLine.Format(_T("%d,\"%s\",\"%s\",%.2f,%lld,\"%s\",\"%s\"\r\n"),
            m_processedCount,
            fileName,
            className,
            confidence * 100,
            processTime,
            savePath,
            timeStamp);

        // 转换为UTF-8
        std::string utf8Line = CT2UTF8(csvLine);
        m_csvFile.write(utf8Line.c_str(), utf8Line.length());
        m_csvFile.flush(); // 实时刷新，确保数据不丢失

    }
    catch (...) {
        // 忽略写入错误，继续处理
    }
}

void CCSVManager::CloseCSVFile()
{
    if (m_csvFile.is_open()) {
        m_csvFile.close();
    }
    m_processedCount = 0;
}

bool CCSVManager::LoadResultsFromCSV(std::map<std::string, std::vector<std::pair<std::string, float>>>& results)
{
    if (m_csvFilePath.IsEmpty() || !PathExists(m_csvFilePath)) {
        return false;
    }

    try {
        std::wstring w_csvFilePath = CStringToWString(m_csvFilePath);
        std::wifstream file(w_csvFilePath);

        if (!file) {
            return false;
        }

        // 清空之前的详细结果
        results.clear();

        std::wstring w_line;
        int lineNumber = 0;

        // 跳过可能的BOM
        wchar_t firstChar = file.get();
        if (firstChar != 0xFEFF) {
            file.putback(firstChar);
        }

        while (std::getline(file, w_line)) {
            lineNumber++;

            // 跳过表头（第一行）
            if (lineNumber == 1) {
                continue;
            }

            if (w_line.empty()) {
                continue;
            }

            CString fileName, className;
            float confidence = 0.0f;

            if (ParseCSVLine(w_line, fileName, className, confidence)) {
                std::string utf8ClassName = CT2UTF8(className);
                std::string utf8FileName = CT2UTF8(fileName);

                // 添加到详细结果中
                if (results.find(utf8ClassName) == results.end()) {
                    results[utf8ClassName] = std::vector<std::pair<std::string, float>>();
                }
                results[utf8ClassName].push_back(std::make_pair(utf8FileName, confidence));
            }
        }

        file.close();
        return !results.empty();

    }
    catch (const std::exception&) {
        return false;
    }
    catch (...) {
        return false;
    }
}

bool CCSVManager::IsFileRecordedInCSV(const CString& fileName)
{
    if (m_csvFilePath.IsEmpty() || !PathExists(m_csvFilePath)) {
        return false;
    }

    try {
        std::wstring w_csvFilePath = CStringToWString(m_csvFilePath);
        std::wifstream file(w_csvFilePath);

        if (!file) {
            return false;
        }

        std::wstring w_line;
        std::wstring w_searchFileName = CStringToWString(fileName);

        // 跳过UTF-8 BOM（如果存在）
        wchar_t bom[3];
        file.read(bom, 3);
        if (bom[0] != 0xFEFF && !(bom[0] == 0xFF && bom[1] == 0xFE)) {
            file.seekg(0); // 不是BOM，回到文件开头
        }

        // 跳过表头
        std::getline(file, w_line);

        while (std::getline(file, w_line)) {
            if (w_line.empty()) continue;

            // 简单的CSV解析：查找文件名（第二列）
            size_t firstComma = w_line.find(L',');
            if (firstComma != std::wstring::npos) {
                size_t secondComma = w_line.find(L',', firstComma + 1);
                if (secondComma != std::wstring::npos) {
                    // 提取文件名（在引号内）
                    size_t quoteStart = w_line.find(L'\"', firstComma + 1);
                    if (quoteStart != std::wstring::npos) {
                        size_t quoteEnd = w_line.find(L'\"', quoteStart + 1);
                        if (quoteEnd != std::wstring::npos) {
                            std::wstring w_recordedFileName = w_line.substr(
                                quoteStart + 1, quoteEnd - quoteStart - 1);

                            if (w_recordedFileName == w_searchFileName) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    catch (...) {
        // 如果读取失败，假设文件不存在记录
    }

    return false;
}

bool CCSVManager::ParseCSVLine(const std::wstring& w_line, CString& fileName, CString& className, float& confidence)
{
    try {
        std::wstring line = w_line;

        // 移除回车符
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }

        std::vector<std::wstring> fields;
        std::wstring field;
        bool inQuotes = false;

        for (size_t i = 0; i < line.length(); i++) {
            wchar_t c = line[i];

            if (c == L'\"') {
                inQuotes = !inQuotes;
            }
            else if (c == L',' && !inQuotes) {
                fields.push_back(field);
                field.clear();
            }
            else {
                field += c;
            }
        }
        fields.push_back(field); // 添加最后一个字段

        // 检查字段数量（应该有7个字段：编号,文件名,类别,置信度,处理耗时,保存路径,处理时间）
        if (fields.size() >= 4) {
            // 文件名（第二个字段，索引1）
            fileName = WStringToCString(fields[1]);

            // 类别（第三个字段，索引2）
            className = WStringToCString(fields[2]);

            // 置信度（第四个字段，索引3）
            std::wstring confidenceStr = fields[3];
            // 移除百分号并转换为浮点数
            if (!confidenceStr.empty()) {
                // 移除可能的百分号
                if (confidenceStr.back() == L'%') {
                    confidenceStr.pop_back();
                }
                confidence = (float)_wtof(confidenceStr.c_str()) / 100.0f;
            }

            return true;
        }

        return false;
    }
    catch (...) {
        return false;
    }
}

// 辅助函数实现
bool CCSVManager::CreateDirectoryRecursive(const CString& path)
{
    if (path.IsEmpty()) return false;

    if (PathExists(path)) return true;

    int pos = path.ReverseFind('\\');
    if (pos != -1) {
        CString parentDir = path.Left(pos);
        if (!CreateDirectoryRecursive(parentDir)) {
            return false;
        }
    }

    return CreateDirectory(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CCSVManager::PathExists(const CString& path)
{
    DWORD attr = GetFileAttributes(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
}

CString CCSVManager::CleanPath(const CString& path)
{
    CString cleanPath = path;
    cleanPath.Trim();

    // 移除末尾的反斜杠
    if (cleanPath.Right(1) == _T("\\")) {
        cleanPath = cleanPath.Left(cleanPath.GetLength() - 1);
    }

    return cleanPath;
}

// 字符编码转换函数
std::string CCSVManager::CT2UTF8(const CString& str)
{
    if (str.IsEmpty()) return std::string();

    int wideLen = str.GetLength();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, str.GetString(), wideLen, NULL, 0, NULL, NULL);
    if (utf8Len == 0) return std::string();

    std::vector<char> buffer(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, str.GetString(), wideLen, buffer.data(), utf8Len, NULL, NULL);

    return std::string(buffer.data(), utf8Len);
}

CString CCSVManager::UTF82CT(const std::string& utf8Str)
{
    if (utf8Str.empty()) return CString();

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wideLen == 0) return CString();

    std::vector<wchar_t> buffer(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, buffer.data(), wideLen);

    return CString(buffer.data());
}

std::wstring CCSVManager::CStringToWString(const CString& str)
{
    return std::wstring(str.GetString());
}

CString CCSVManager::WStringToCString(const std::wstring& wstr)
{
    return CString(wstr.c_str());
}