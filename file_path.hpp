/**
 * @file path.hpp
 * @author okano tomoyuki (tomoyuki.okano@tsuneishi.com)
 * @brief 
 * @version 0.1
 * @date 2024-02-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef _UTILITY_FILE_PATH_HPP_
#define _UTILITY_FILE_PATH_HPP_

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cstring>

#include <sys/stat.h>

#ifdef __unix__
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include <linux/limits.h>
#else
#include <windows.h>
#include <fileapi.h>
#endif

/**
 * @class FilePath
 * @brief c++11/c++14 環境におけるstd::filesystem 名前空間の代替クラス 
 * 
 */
class FilePath
{
public:
    enum Platform
    {
        UNIX    = 0,
        WINDOWS = 1,
#ifdef __unix__
        NATIVE = UNIX
#else
        NATIVE = WINDOWS
#endif
    };

    FilePath()
     :  platform_(NATIVE), is_absolute_(false) 
    {}

    FilePath(const FilePath &path)
     :  platform_(path.platform_), path_(path.path_), is_absolute_(path.is_absolute_)
    {}

    FilePath(const char* path_str)
    { 
        set(path_str); 
    }

    FilePath(const std::string& path_str) 
    { 
        set(path_str); 
    }

    FilePath(const std::wstring& path_str)
    {
        set(path_str);
    }

    void operator=(const FilePath &path)
    {
        platform_       = path.platform_;
        path_           = path.path_;
        is_absolute_    = path.is_absolute_;
    }

    void operator=(const char* path_str)
    {
        set(path_str);
    }

    void operator=(const std::string& path_str)
    {
        set(path_str);
    }

    FilePath operator/(const FilePath &other) const
    {
        if (other.is_absolute_)
            throw std::runtime_error("FilePath::operator/(): expected a relative path!");
        if (platform_ != other.platform_)
            throw std::runtime_error("FilePath::operator/(): expected a path of the same type!");

        FilePath result(*this);

        for (size_t i = 0; i < other.path_.size(); ++i)
            result.path_.push_back(other.path_[i]);

        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const FilePath& path)
    {
        os << path.to_str();
        return os;
    }

    /**
     * @brief 
     * 
     * @return int64_t 
     */
    int64_t file_size() const 
    {
#ifdef __unix__
        struct stat st;
        if(!is_file() || stat(to_str().c_str(), &st) != 0)
        {
            return -1;
        }
        return st.st_size;
#else
        HANDLE handle = CreateFileA(to_str().c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE) 
        {
            return -1;
        }
        LARGE_INTEGER size;
        if (!GetFileSizeEx(handle, &size)) 
        {
            CloseHandle(handle);
            return -1;
        }
        return size.QuadPart;
#endif
    }

    bool empty() const 
    { 
        return path_.empty(); 
    }

    bool is_absolute() const 
    { 
        return is_absolute_; 
    }

    bool is_directory() const
    {
#ifdef __unix__
        struct stat sb;
        if (stat(to_str().c_str(), &sb))
            return false;
        return S_ISDIR(sb.st_mode);
#else
        DWORD attr = GetFileAttributesA(to_str().c_str());
        return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
#endif
    }

    bool is_file() const
    {
#ifdef __unix__
        struct stat sb;
        if (stat(to_str().c_str(), &sb))
            return false;
        return S_ISREG(sb.st_mode);
#else
        DWORD attr = GetFileAttributesA(to_str().c_str());
        return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
#endif
    }

    FilePath make_absolute() const
    {
#ifdef __unix__
        char tmp[PATH_MAX];
        if (realpath(to_str().c_str(), tmp) == NULL)
        {
            std::stringstream ss;
            ss << "Internal error in " << __func__ << " : " << errno;
            throw std::runtime_error(ss.str());
        }
        return FilePath(tmp);
#else
        std::string tmp(MAX_PATH, '\0');
        DWORD length = GetFullPathNameA(to_str().c_str(), MAX_PATH, &tmp[0], NULL);
        if (length == 0)
        {
            std::stringstream ss;
            ss << "Internal error in " << __func__ << ":" << GetLastError();
            throw std::runtime_error(ss.str());
        }
        return FilePath(tmp.substr(0, length));
#endif
    }

    bool exists() const
    {
#ifdef __unix__
        struct stat sb;
        return stat(to_str().c_str(), &sb) == 0;
#else
        return GetFileAttributesA(to_str().c_str()) != INVALID_FILE_ATTRIBUTES;
#endif
    }

    std::string to_str(Platform platform = NATIVE) const
    {
        std::stringstream ss;

        if (is_absolute_)
        {
            if (platform_ == UNIX)
                ss << "/";
        }

        for(const auto& p : path_)
        {
            if(platform == UNIX)
                ss << p << '/';
            else
                ss << p << '\\';
        }

        std::string result = ss.str();
        result.pop_back();
        return result;
    }

    std::wstring to_wstr(const Platform& platform = NATIVE) const
    {
#ifdef __unix__
        return std::wstring();
#else
        std::string tmp = to_str(platform);
        int size = MultiByteToWideChar(CP_UTF8, 0, &tmp[0], tmp.size(), NULL, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, &tmp[0], tmp.size(), &result[0], size);
        return result;
#endif

    }

    /**
     * @brief 
     * 
     * @param path 
     * @return std::vector<FilePath> 
     */
    static std::vector<FilePath> directory_iterator(const FilePath& path)
    {
        if(!path.is_directory())
            return {};
        auto result = std::vector<FilePath>();
#ifdef __unix__
        DIR* dir = opendir(path.to_str().c_str());
        if(dir == NULL)
            return {};
        struct dirent* dp;
        while ((dp = readdir(dir)) != NULL)
        {
            result.push_back(path / dp->d_name);
        }
        closedir(dir);
#else
        WIN32_FIND_DATAA win32fd;
        HANDLE handle = FindFirstFileA((path / "*").to_str().c_str() , &win32fd);
        if (handle == INVALID_HANDLE_VALUE) 
        {
            FindClose(handle);
            return {};
        }
        do 
        {
            std::string file(win32fd.cFileName);
            result.push_back(path / file);
        } while (FindNextFileA(handle, &win32fd));
        FindClose(handle);
#endif
        return result;
    }

    /**
     * @brief 対象ファイルの拡張子を取得する
     * 
     * @return std::string 
     */
    std::string extension() const
    {
        if(!is_directory())
            return "";
        const std::string name = filename();
        size_t pos = name.find_last_of('.');
        if (pos == std::string::npos)
            return "";
        return name.substr(pos + 1);
    }

    std::string filename() const
    {
        if (empty())
            return "";
        return path_.back();
    }

    FilePath parent_path() const
    {
        FilePath result = *this;
        if(!result.empty() && result.path_.back() != "." && result.path_.back() != "..")
            result.path_.pop_back();
        else
            result.path_.push_back("..");

        if(is_absolute_)
        {
            return result.make_absolute();
        }
        return result;
    }

    bool remove_file() const
    {
#ifdef __unix__
        return std::remove(to_str().c_str()) == 0;
#else
        return DeleteFileA(to_str().c_str()) != 0;
#endif
    }

    bool resize_file(size_t target_length)
    {
#ifdef __unix__
        return truncate(to_str().c_str(), (off_t)target_length) == 0;
#else
        HANDLE handle = CreateFileA(to_str().c_str(), GENERIC_WRITE, 0, nullptr, 0, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        LARGE_INTEGER size;
        size.QuadPart = (LONGLONG)target_length;
        if (SetFilePointerEx(handle, size, NULL, FILE_BEGIN) == 0)
        {
            CloseHandle(handle);
            return false;
        }
        if (SetEndOfFile(handle) == 0)
        {
            CloseHandle(handle);
            return false;
        }
        CloseHandle(handle);
        return true;
#endif
    }

    static FilePath current_path()
    {
#ifdef __unix__
        char tmp[PATH_MAX];
        if (getcwd(tmp, PATH_MAX) == NULL)
        {
            std::stringstream ss;
            ss << "Internal error in current_path(): " << errno;
            throw std::runtime_error(ss.str());
        }
        return FilePath(tmp);
#else
        char tmp[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, tmp) == NULL)
        {
            std::stringstream ss;
            ss << "Internal error in current_path(): " << GetLastError();
            throw std::runtime_error(ss.str());
        }
        FilePath path;
        path.set(tmp);
        return path;
#endif
    }

    static bool create_directory(const FilePath &path)
    {
#ifdef __unix__
        return mkdir(path.to_str().c_str(), S_IRWXU) == 0;
#else
        return CreateDirectoryA(path.to_str().c_str(), NULL) != 0;
#endif
    }

    /**
     * @fn application_path
     * @brief 実行ファイルの絶対パスを取得するメソッド
     * 
     * @return FilePath 実行ファイルの絶対パス
     */
    static FilePath application_path()
    {
#ifdef __unix__
        char tmp[PATH_MAX];
        if (readlink("/proc/self/exe", tmp, sizeof(tmp)-1) == -1)
        {
            std::stringstream ss;
            ss << "Internal error in " << __func__ << " : " << errno;
            throw std::runtime_error(ss.str());
        }
        return FilePath(tmp);
#else
        char tmp[MAX_PATH];
        GetModuleFileNameA(NULL, tmp, MAX_PATH);
        return FilePath(tmp);
#endif      
    }

private:
    std::vector<std::string> path_;     /**< */
    bool is_absolute_;                  /**< */
    Platform platform_;

    static std::vector<std::string> split(const std::string& origin, const std::string &delim)
    {
        std::vector<std::string> result;
        std::size_t last_pos = 0;
        std::size_t find_pos = origin.find_first_of(delim, last_pos);

        while (last_pos != std::string::npos)
        {
            if (find_pos != last_pos)
                result.push_back(origin.substr(last_pos, find_pos - last_pos));
            last_pos = find_pos;
            if (last_pos == std::string::npos || last_pos == origin.size()-1)
                break;
            last_pos++;
            find_pos = origin.find_first_of(delim, last_pos);
        }

        return result;
    }

    void set(const std::string& path_str, Platform platform = NATIVE)
    {
        platform_ = platform;
        if (platform == WINDOWS)
        {
            std::string tmp = path_str;

            if (tmp.size() >= 3 && (static_cast<u_char>(tmp[0]) < 0x80) && std::isalpha(tmp[0]) && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/'))
            {
                path_ = {tmp.substr(0, 2)};
                tmp.erase(0, 3);
                is_absolute_ = true;
            }
            else
            {
                path_ = {};
                is_absolute_ = false;
            }

            std::vector<std::string> splited = split(tmp, "/\\");
            path_.insert(std::end(path_), std::begin(splited), std::end(splited));
        }
        else
        {
            path_ = split(path_str, "/");
            is_absolute_ = !path_str.empty() && path_str[0] == '/';
        }
    }

    void set(const std::wstring &wstring, const Platform platform = NATIVE) 
    {
#ifdef __unix__

#else
        std::string string;
        if (!wstring.empty()) 
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(), NULL, 0, NULL, NULL);
            string.resize(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(), &string[0], size, NULL, NULL);
        }
        set(string, platform);
#endif
    }

};

#endif