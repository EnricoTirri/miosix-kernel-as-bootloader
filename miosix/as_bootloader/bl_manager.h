// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "kernelfiles/kfile.h"
#include "kernelfiles/kfilefactory.h"

namespace miosix
{
    class BootloaderManager
    {
    public:
        explicit BootloaderManager(const std::string &mountpoint, const std::string &kernelsDir);

        BootloaderManager(const std::string &mountpoint) : BootloaderManager(mountpoint, "/kernels/") {}

        BootloaderManager() : BootloaderManager("/sd/") {}

        bool isValid() const { return valid; }

        void loadConfig();

        std::shared_ptr<KernelFile> selectFile();

        const std::vector<std::shared_ptr<KernelFile>> &getKernelFiles() const
        {
            return kernelFiles;
        }

    private:
        std::string mountpoint;
        std::string kernelsDir;
        
        bool valid;
        std::vector<std::shared_ptr<KernelFile>> kernelFiles;

        size_t getFileSize(const std::string &filepath);
    };
}