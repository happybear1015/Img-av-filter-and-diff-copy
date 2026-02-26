// FileUtils.h
#pragma once

class FileUtils
{
public:
    static std::string CT2UTF8(const CString& str);
    static CString UTF82CT(const std::string& utf8Str);
    static std::wstring CStringToWString(const CString& str);
    static CString WStringToCString(const std::wstring& wstr);

    static bool CreateDirectoryRecursive(const CString& path);
    static bool PathExists(const CString& path);
    static CString BrowseForFolder(const CString& title = _T("Ñ¡ÔñÎÄ¼þ¼Ð"));
    static CString BrowseForFile(const CString& filter, const CString& title);

    static std::vector<CString> GetImageFiles(const CString& folderPath);
    static bool IsImageFile(const CString& filePath);
    static CString GetFileName(const CString& filePath);
};