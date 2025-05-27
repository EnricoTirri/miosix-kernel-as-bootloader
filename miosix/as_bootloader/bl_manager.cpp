// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#include "bl_manager.h"
#include <filesystem>
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

namespace miosix
{
    BootloaderManager::BootloaderManager(const std::string &mountpoint)
        : mountpoint(mountpoint), valid(false)
    {
        try
        {
            if (!fs::exists(mountpoint) || !fs::is_directory(mountpoint))
            {
                printf("Mountpoint does not exist or is not a directory: %s\n", mountpoint.c_str());
                valid = false;
                return;
            }

            valid = true;

            for (const auto &entry : fs::directory_iterator(mountpoint))
            {
                if (entry.is_regular_file())
                {
                    std::string filename = entry.path().filename().string();
                    std::string fullpath = entry.path().string();
                    size_t filesize = getFileSize(fullpath);

                    // Create KernelFile using factory, passing directory and filename, plus size
                    auto kfile = KernelFileFactory::instance().create(mountpoint, filename, filesize);
                    kernelFiles.push_back(std::move(kfile));
                }
            }
        }
        catch (const std::exception &e)
        {
            printf("Exception while opening directory: %s", e.what());
            valid = false;
        }
    }

    size_t BootloaderManager::getFileSize(const std::string &filepath)
    {
        try
        {
            return fs::file_size(filepath);
        }
        catch (...)
        {
            printf("Failed to get size of file: %s\n", filepath.c_str());
            return -1;
        }
    }

}