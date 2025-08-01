// TODO must edit: LICENSE

#include <cstring>
#include <stdexcept>

#include "dirent.h"
#include "sys/stat.h"

#include "filesystem/file_access.h"
#include "kernel/logging.h"
#include "bl_manager.h"

#ifdef WITH_FATFS
namespace miosix
{
    BootloaderManager::BootloaderManager(const std::string &mountpoint, bool loadConfig)
    {
        bootloaderlog("Initializing Bootloader Manager ...\n");

        setMountpoint(mountpoint, loadConfig);

        loadKernelsDir();

        bootloaderlog("... DONE : %u kernel files\n", kernelFiles.size());
    }

    BootloaderManager &BootloaderManager::setMountpoint(const std::string &mountpoint, bool loadConfig)
    {
        this->mountpoint = mountpoint;
        bootloaderlog("Mountpoint set to: %s\n", mountpoint.c_str());

        if (loadConfig)
        {
            this->loadConfig();
        }

        return *this;
    }

    void BootloaderManager::loadKernelsDir()
    {
        try
        {
            // Check if kernelsDir is valid
            std::string filesDir = mountpoint + "/" + kernelsDir;
            DIR *dir = opendir(filesDir.c_str());
            if (!dir)
                throw std::runtime_error("Kernel files directory does not exist or is not a directory");

            // Build the list of kernel files from the directory
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                // Skip the current and parent directory entries
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                std::string filename = entry->d_name;
                std::string fullpath = filesDir + "/" + filename;

                struct stat st;
                if (stat(fullpath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
                {
                    size_t filesize = st.st_size;
                    auto kfile = KernelFileFactory::instance().create(filesDir, filename, filesize);
                    kernelFiles.push_back(std::move(kfile));
                }
            }
            closedir(dir);

            if (kernelFiles.empty())
                throw new std::runtime_error("No kernel files found in directory");
        }
        catch (const std::exception &e)
        {
            bootloaderlog("KO : %s\n", e.what());
            return;
        }

        // Check if autorun file exists, if found set as selected
        if (autorun != "")
        {
            for (size_t i = 0; i < kernelFiles.size(); ++i)
            {
                if (autorun == kernelFiles[i]->getFilename())
                {
                    selectedFile = i;
                    bootloaderlog("Autorun file found: %s\n", kernelFiles[i]->getFilename().c_str());
                    return;
                }
            }
        }

        return;
    }

    size_t BootloaderManager::getFileSize(const std::string &filepath)
    {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0)
            return st.st_size;
        return static_cast<size_t>(-1);
    }

    BootloaderManager &BootloaderManager::loadConfig()
    {
        bootloaderlog("Loading configuration ... ");
        bool oldVerbose = verbose; // Save old verbose state

        // Load configuration from the config.txt file in the mountpoint
        std::string configPath = mountpoint + "/config.txt";

        FILE *configFile = fopen(configPath.c_str(), "r");
        if (!configFile)
        {
            bootloaderlog("KO continuing with defaults\n");
            return *this;
        }

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), configFile))
        {
            // Remove trailing newline character
            buffer[strcspn(buffer, "\n")] = '\0';

            // Find the delimiter ':' in the line
            char *delimiter = strchr(buffer, ':');
            if (delimiter == nullptr)
                continue;

            // Split the line into tag and value
            *delimiter = '\0'; // Replace ':' with null terminator
            std::string tag = buffer;
            std::string value = delimiter + 1;

            // try to assign the value to tagged variable
            assignTag(tag, value);
        }
        fclose(configFile);

        if (oldVerbose)
            bootloaderlog("OK\n");
        return *this;
    }

    void BootloaderManager::loadSelectedFile()
    {
        if (selectedFile == -1)
        {
            bootloaderlog("No kernel file selected, cannot load\n");
            return;
        }

        if (kernelFiles[selectedFile] == nullptr)
        {
            bootloaderlog("Selected kernel file does not exists, cannot load\n");
            return;
        }

        bootloaderlog("Loading kernel : %s ... ", kernelFiles[selectedFile]->getFilename().c_str());

        relocationAddress = nullptr;
        kernelFileStart = nullptr;
        kernelFileEnd = nullptr;

        // Load the selected kernel file into memory
        try
        {
            kernelFiles[selectedFile]->load(&relocationAddress, &kernelFileStart, &kernelFileEnd);
        }
        catch (const std::exception &e)
        {
            bootloaderlog("KO : %s\n", e.what());
            relocationAddress = nullptr;
            kernelFileStart = nullptr;
            kernelFileEnd = nullptr;
            return;
        }
        catch (...)
        {
            bootloaderlog("KO : unknown error\n");
            relocationAddress = nullptr;
            kernelFileStart = nullptr;
            kernelFileEnd = nullptr;
            return;
        }

        bootloaderlog("OK\n\t - Loaded from %p to %p.\n\t - Relocation at %p\n", kernelFileStart, kernelFileEnd, relocationAddress);
    }

    void BootloaderManager::boot()
    {
        loadSelectedFile();

        if (kernelFileStart == nullptr || kernelFileEnd == nullptr || relocationAddress == nullptr)
        {
            bootloaderlog("Kernel file not loaded, cannot boot\n");
            return;
        }

        bootloaderlog("! Booting kernel file\n");

        GlobalIrqLock lock;
        bootloaderlog("! GlobalLock acquired\n");

        FilesystemManager &fs = FilesystemManager::instance();

        IRQbootloaderlog("! Unmounting sda ... ");
        if (fs.getDevFs()->remove("sda"))
        {
            IRQbootloaderlog("OK\r\n");
        }
        else
        {
            IRQbootloaderlog("KO\r\n");
        }

        IRQbootloaderlog("! Unmounting all filesystems ... ");
        fs.umountAll();
        IRQbootloaderlog("DONE\r\n");

        IRQbootloaderlog("! Checking all file are closed ... ");
        if (fs.getDevFs()->areAllFilesClosed())
        {
            IRQbootloaderlog("OK\r\n");
        }
        else
        {
            IRQbootloaderlog("KO\r\n");
        }

        IRQbootloaderlog("! Setting up and running kernel ...\r\n\n");
        copyRun(relocationAddress, kernelFileStart, kernelFileEnd);
    }

    [[noreturn]] void __attribute__((naked)) BootloaderManager::copyRun(void *relocationAddress, void *kernelFileStart, void *kernelFileEnd)
    {
        __asm__ __volatile__(
            "cpsid i            \n\t" // Disable interrupts
            "mov r0, %[reloc]   \n\t" // Relocation address / pointer to main stack pointer value
            // Copy the kernel file to relocation address
            "mov r1, r0         \n\t" // Dest
            "mov r2, %[start]   \n\t" // Source start
            "mov r3, %[end]     \n\t" // Source end
            "cmp r2, r3         \n\t" // Check if start != end
            "beq 2f             \n\t" // If size = 0 skip copy
            // Copy loop
            "1:                 \n\t"
            "ldrb r4, [r2], #1  \n\t" // Load byte from source and increment source pointer
            "strb r4, [r1], #1  \n\t" // Store byte to destination and increment destination pointer
            "cmp r2, r3         \n\t" // Check if we reached the end
            "bne 1b             \n\t" // If not, repeat
            // Simulate hardware reset
            "2:                 \n\t"
            "ldr r4, [r0]       \n\t" // Load value at relocation address (initial MSP) into r4
            "msr msp, r4        \n\t" // Set the main stack pointer to value at the relocation address
            "add r0, r0, #4     \n\t" // second word of file
            "ldr r0, [r0]       \n\t" // Get address of reset handler function
            "bx r0              \n\t" // Call reset handler
            :
            : [reloc] "r"(relocationAddress),
              [start] "r"(kernelFileStart),
              [end] "r"(kernelFileEnd)
            : "r0", "r1", "r2", "r3", "r4", "memory");

        // This point should never be reached
        bootloaderlog("KERNEL BOOT FAILED\n");
    }

    void BootloaderManager::assignTag(const std::string &tag, const std::string &value)
    {
#define CHECK_TAG(tagVar, tagVal, valueDst, valueSrc) \
    {                                                 \
        if (tagVar == tagVal)                         \
        {                                             \
            valueDst = valueSrc;                      \
            return;                                   \
        }                                             \
    }
        CHECK_TAG(tag, "autorun", autorun, value)
        CHECK_TAG(tag, "kernelsDir", kernelsDir, value)
        CHECK_TAG(tag, "verbose", verbose, (value == "1"))

#undef CHECK_TAG
    }

    void BootloaderManager::bootloaderlog(const char *fmt, ...)
    {
        if (!verbose)
            return;

        va_list arg;
        va_start(arg, fmt);
        viprintf(fmt, arg);
        va_end(arg);
    }

    void BootloaderManager::IRQbootloaderlog(const char *fmt)
    {
        if (!verbose)
            return;

        miosix::DefaultConsole::instance().IRQget()->IRQwrite(fmt);
    }
}

#endif // WITH_FATFS