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

        void assignTag(const std::string &tag, const std::string &value);

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

// Configs definer
#define CONFIG_VAR(type, name, default) \
private:                                \
    type name = default;                \
                                        \
public:                                 \
    type get##name() const { return name; }

        // Config list
        CONFIG_VAR(std::string, DefaultFile, "")
        CONFIG_VAR(std::string, AlternativeFile, "")
        CONFIG_VAR(bool, Verbose, false)

#undef CONFIG_VAR
    };
}