// TODO must edit: LICENSE

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "kernelfiles/kfile.h"
#include "kernelfiles/kfilefactory.h"

#ifdef WITH_FATFS
namespace miosix
{
    class BootloaderManager
    {
    public:
        // Constructor with mountpoint and kernel files directory
        explicit BootloaderManager(const std::string &mountpoint, bool loadConfig = true);

        // Default constructor with default mountpoint and kernel files directory
        BootloaderManager(bool loadConfig = true) : BootloaderManager("/sd/", loadConfig) {}

        // Change the mountpoint where the bootloader will look for kernel files
        BootloaderManager &setMountpoint(const std::string &mountpoint, bool loadConfig = true);

        // Make bootloader load config from mountpoint/config.txt
        BootloaderManager &loadConfig();

        std::vector<std::string> getKernelFiles() const
        {
            std::vector<std::string> fileNames;
            for (const auto &file : kernelFiles)
            {
                fileNames.push_back(file->getFilename());
            }
            return fileNames;
        }

        // Select a kernel file to boot
        bool selectFile(size_t index = 0)
        {
            if (index < 0 || index >= kernelFiles.size()){
                return false; // Invalid index, do not select
            }

            selectedFile = index;
            return true;
        }

        bool isFileSelected() const
        {
            return selectedFile != -1;
        }

        // Boot the loaded kernel file
        void boot();

    private:
        // Mountpoint where config and kernelDir are located
        std::string mountpoint;

        // List of kernel files found in the kernels directory
        std::vector<std::unique_ptr<KernelFile>> kernelFiles;

        // Selected kernel file
        int selectedFile = -1;

        // Pointers to the start and end of the loaded kernel file in memory and the relocation address
        void *kernelFileStart = nullptr,
             *kernelFileEnd = nullptr,
             *relocationAddress = nullptr;

        // Load the kernel files available from bootloader resources
        void loadKernelsDir();

        // Loads the selected kernel file into memory
        void loadSelectedFile();

        // Util function that checks if tag exists and assign value to its variable
        void assignTag(const std::string &tag, const std::string &value);

        // Util function to get the size of a file
        size_t getFileSize(const std::string &filepath);

        // Util functions to print bootloader logs
        void bootloaderlog(const char *fmt, ...);
        void IRQbootloaderlog(const char *fmt);

        // Function to copy the kernel file to the relocation address and jump to the reset handler
        [[noreturn]] void __attribute__((naked)) copyRun(void *relocationAddress, void *kernelFileStart, void *kernelFileEnd);

        // CONFIGURATION VARIABLES //

// Configs definer macro, create a private variable with a getter method
#define CONFIG_VAR(type, name, default) \
private:                                \
    type name = default;                \
                                        \
public:                                 \
    type getT##name() const { return name; }

        // Config list
        CONFIG_VAR(std::string, autorun, "")     // Default file to boot
        CONFIG_VAR(std::string, kernelsDir, "kernels") // Directory where kernel files are located
        CONFIG_VAR(bool, verbose,
#ifdef WITH_BOOTLOG
                   true
#else
                   false
#endif
                   ) // Verbose mode, default based on WITH_BOOTLOG

#undef CONFIG_VAR
    };
}

#endif // WITH_FATFS