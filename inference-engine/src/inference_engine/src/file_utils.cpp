﻿// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <cstring>
#include <fstream>
#include <string>

#ifdef __MACH__
#    include <mach/clock.h>
#    include <mach/mach.h>
#endif

#include <file_utils.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ie_common.h"
#ifndef _WIN32
#    include <dlfcn.h>
#    include <limits.h>
#    include <unistd.h>
#    ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
#        include <codecvt>
#        include <locale>
#    endif
#else
#    if defined(WINAPI_FAMILY) && !WINAPI_PARTITION_DESKTOP
#        error "Only WINAPI_PARTITION_DESKTOP is supported, because of GetModuleHandleEx[A|W]"
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <Windows.h>
#endif

#ifdef _WIN32

#    include <direct.h>

// Copied from linux libc sys/stat.h:
#    define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)

/// @brief Windows-specific 'mkdir' wrapper
#    define makedir(dir) _mkdir(dir)

/// @brief Max length of absolute file path
#    define MAX_ABS_PATH _MAX_PATH
/// @brief Get absolute file path, returns NULL in case of error
#    define get_absolute_path(result, path) _fullpath(result, path.c_str(), MAX_ABS_PATH)

/// @brief Windows-specific 'stat' wrapper
#    define stat _stat

#else

#    include <unistd.h>

/// @brief mkdir wrapper
#    define makedir(dir)                    mkdir(dir, 0755)

/// @brief Max length of absolute file path
#    define MAX_ABS_PATH                    PATH_MAX
/// @brief Get absolute file path, returns NULL in case of error
#    define get_absolute_path(result, path) realpath(path.c_str(), result)

#endif

long long FileUtils::fileSize(const char* charfilepath) {
#if defined(OPENVINO_ENABLE_UNICODE_PATH_SUPPORT) && defined(_WIN32)
    std::wstring widefilename = ov::util::string_to_wstring(charfilepath);
    const wchar_t* fileName = widefilename.c_str();
#elif defined(__ANDROID__) || defined(ANDROID)
    std::string fileName = charfilepath;
    std::string::size_type pos = fileName.find('!');
    if (pos != std::string::npos) {
        fileName = fileName.substr(0, pos);
    }
#else
    const char* fileName = charfilepath;
#endif
    std::ifstream in(fileName, std::ios_base::binary | std::ios_base::ate);
    return in.tellg();
}

std::string FileUtils::absoluteFilePath(const std::string& filePath) {
    std::string absolutePath;
    absolutePath.resize(MAX_ABS_PATH);
    auto absPath = get_absolute_path(&absolutePath[0], filePath);
    if (!absPath) {
        IE_THROW() << "Can't get absolute file path for [" << filePath << "], err = " << strerror(errno);
    }
    absolutePath.resize(strlen(absPath));
    return absolutePath;
}

bool FileUtils::directoryExists(const std::string& path) {
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    }
    return false;
}

void FileUtils::createDirectoryRecursive(const std::string& dirPath) {
    if (dirPath.empty() || directoryExists(dirPath)) {
        return;
    }

    std::size_t pos = dirPath.rfind(ov::util::FileTraits<char>::file_separator);
    if (pos != std::string::npos) {
        createDirectoryRecursive(dirPath.substr(0, pos));
    }

    int err = makedir(dirPath.c_str());
    if (err != 0 && errno != EEXIST) {
        // TODO: in case of exception it may be needed to remove all created sub-directories
        IE_THROW() << "Couldn't create directory [" << dirPath << "], err=" << strerror(errno) << ")";
    }
}

namespace InferenceEngine {

namespace {

template <typename C, typename = InferenceEngine::details::enableIfSupportedChar<C>>
std::basic_string<C> getPathName(const std::basic_string<C>& s) {
    size_t i = s.rfind(ov::util::FileTraits<C>::file_separator, s.length());
    if (i != std::string::npos) {
        return (s.substr(0, i));
    }

    return {};
}

}  // namespace

static std::string getIELibraryPathA() {
#ifdef _WIN32
    CHAR ie_library_path[MAX_PATH];
    HMODULE hm = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPSTR>(getIELibraryPath),
                            &hm)) {
        IE_THROW() << "GetModuleHandle returned " << GetLastError();
    }
    GetModuleFileNameA(hm, (LPSTR)ie_library_path, sizeof(ie_library_path));
    return getPathName(std::string(ie_library_path));
#elif defined(__APPLE__) || defined(__linux__)
#    if defined(OPENVINO_STATIC_LIBRARY) || defined(USE_STATIC_IE)
#        ifdef __APPLE__
    Dl_info info;
    dladdr(reinterpret_cast<void*>(getIELibraryPath), &info);
    std::string path = getPathName(std::string(info.dli_fname)).c_str();
#        else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string path = getPathName(std::string(result, (count > 0) ? count : 0));
#        endif  // __APPLE__
    return FileUtils::makePath(path, std::string("lib"));
#    else
    Dl_info info;
    dladdr(reinterpret_cast<void*>(getIELibraryPath), &info);
    return getPathName(std::string(info.dli_fname)).c_str();
#    endif  // OPENVINO_STATIC_LIBRARY || USE_STATIC_IE
#else
#    error "Unsupported OS"
#endif  // _WIN32
}

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

std::wstring getIELibraryPathW() {
#    ifdef _WIN32
    WCHAR ie_library_path[MAX_PATH];
    HMODULE hm = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(getIELibraryPath),
                            &hm)) {
        IE_THROW() << "GetModuleHandle returned " << GetLastError();
    }
    GetModuleFileNameW(hm, (LPWSTR)ie_library_path, sizeof(ie_library_path) / sizeof(ie_library_path[0]));
    return getPathName(std::wstring(ie_library_path));
#    elif defined(__linux__) || defined(__APPLE__)
    return ::ov::util::string_to_wstring(getIELibraryPathA().c_str());
#    else
#        error "Unsupported OS"
#    endif
}

#endif  // OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

std::string getIELibraryPath() {
#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
    return ov::util::wstring_to_string(getIELibraryPathW());
#else
    return getIELibraryPathA();
#endif
}

}  // namespace InferenceEngine
