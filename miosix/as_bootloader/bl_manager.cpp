// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#include "bl_manager.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace miosix
{
    BootloaderManager::BootloaderManager(const std::string &mountpoint)
        : mountpoint(mountpoint), valid(false)
    {
        DIR *dir = opendir(mountpoint.c_str());
        if (!dir)
        {
            printf("Mountpoint does not exist or is not a directory: %s\n", mountpoint.c_str());
            valid = false;
            return;
        }

        valid = true;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            std::string filename = entry->d_name;
            std::string fullpath = mountpoint + "/" + filename;

            struct stat st;
            if (stat(fullpath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            {
                size_t filesize = st.st_size;
                auto kfile = KernelFileFactory::instance().create(mountpoint, filename, filesize);
                kernelFiles.push_back(kfile);
            }
        }
        closedir(dir);
    }

    size_t BootloaderManager::getFileSize(const std::string &filepath)
    {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0)
            return st.st_size;
        printf("Failed to get size of file: %s\n", filepath.c_str());
        return static_cast<size_t>(-1);
    }

    std::shared_ptr<KernelFile> BootloaderManager::selectFile()
    {
        // TODO skip selection if specified in config

        // print all kernel files name
        printf("Available kernel files:\n");
        int i = 0;
        for (auto file : kernelFiles)
        {
            printf(" %d) %s\n", i++, file->getFilename().c_str());
        }
        size_t selected = -1;
        while (selected < 0 || selected >= kernelFiles.size())
        {
            printf("Select an index: "); fflush(stdout);
            scanf("%u", &selected);
        }

        return kernelFiles.at(selected);
    }

}