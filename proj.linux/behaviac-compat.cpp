// behaviac GCC/Linux 兼容实现
// filesystem_gcc.cpp 引用了 ListFiles_internal 但未提供实现
#include "behaviac/common/string/stringutils.h"
#include "behaviac/common/memory/memory.h"
#include "behaviac/common/container/vector.h"
#include <dirent.h>
#include <sys/stat.h>

void ListFiles_internal(behaviac::vector<behaviac::string>& files,
                        const char* szDirName, bool bRecursive)
{
    DIR* dir = opendir(szDirName);
    if (!dir) return;

    bool endsWithSlash = behaviac::StringUtils::EndsWith(szDirName, "/");

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        behaviac::string fileName;
        if (endsWithSlash)
            fileName = behaviac::FormatString("%s%s", szDirName, entry->d_name);
        else
            fileName = behaviac::FormatString("%s/%s", szDirName, entry->d_name);

        if (entry->d_type == DT_DIR && bRecursive) {
            files.push_back(fileName);
            ListFiles_internal(files, fileName.c_str(), true);
        } else if (entry->d_type != DT_DIR) {
            files.push_back(fileName);
        } else if (entry->d_type == DT_UNKNOWN) {
            // 回退到 stat
            struct stat st;
            std::string fullPath;
            if (endsWithSlash)
                fullPath = std::string(szDirName) + entry->d_name;
            else
                fullPath = std::string(szDirName) + "/" + entry->d_name;
            if (stat(fullPath.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode) && bRecursive) {
                    files.push_back(fileName);
                    ListFiles_internal(files, fileName.c_str(), true);
                } else if (S_ISREG(st.st_mode)) {
                    files.push_back(fileName);
                }
            }
        }
    }
    closedir(dir);
}
