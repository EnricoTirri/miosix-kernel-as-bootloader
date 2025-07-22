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
        explicit BootloaderManager(const std::string &mountpoint, const std::string &kernelsDir, bool loadConfig = true);

        // Constructor with mountpoint and default kernel files directory
        BootloaderManager(const std::string &mountpoint, bool loadConfig = true) : BootloaderManager(mountpoint, "/kernels/", loadConfig) {}

        // Default constructor with default mountpoint and kernel files directory
        BootloaderManager(bool loadConfig = true) : BootloaderManager("/sd/", loadConfig) {}

        // Returns if the bootloader manager has been initialized correctly
        bool isValid() const { return valid; }

        // Make bootloader load config from mountpoint/config.txt
        BootloaderManager &loadConfig();

        // Select a kernel file to boot
        BootloaderManager &selectFile();

        // Loads the selected kernel file into memory
        BootloaderManager &loadSelectedFile();

        // Boot the loaded kernel file
        void boot();

    private:
        // Mountpoint where config and kernelDir are located
        std::string mountpoint;
        // Directory where kernel files are located
        std::string kernelsDir;

        // Indicates if the bootloader manager has been initialized correctly
        bool valid;

        // List of kernel files found in the kernels directory
        std::vector<std::unique_ptr<KernelFile>> kernelFiles;

        // Selected kernel file
        size_t selectedFile = -1;

        // Pointers to the start and end of the loaded kernel file in memory and the relocation address
        void *kernelFileStart = nullptr,
             *kernelFileEnd = nullptr,
             *relocationAddress = nullptr;

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

// Configs definer macro, create a private variable with a getter
#define CONFIG_VAR(type, name, default) \
private:                                \
    type name = default;                \
                                        \
public:                                 \
    type get##name() const { return name; }

        // Config list
        CONFIG_VAR(std::string, DefaultFile, "")     // Default file to boot
        CONFIG_VAR(std::string, AlternativeFile, "") // Alternative file to boot if the default is not found
        CONFIG_VAR(bool, Verbose,
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