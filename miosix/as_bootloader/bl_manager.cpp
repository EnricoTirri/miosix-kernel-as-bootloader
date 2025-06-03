// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#include "bl_manager.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <filesystem/file_access.h>
#include "kernel/logging.h"

#ifdef AS_BOOTLOADER

namespace miosix
{
    BootloaderManager::BootloaderManager(const std::string &mountpoint, const std::string &kernelsDir, bool loadConfig)
        : mountpoint(mountpoint), kernelsDir(kernelsDir), valid(false)
    {
        // Check if mountpoint is a valid directory
        bootlog("Checking mountpoint: %s ... ", mountpoint.c_str());
        DIR *mountDir = opendir(mountpoint.c_str());
        if (!mountDir)
        {
            bootlog("KO : does not exist or is not a directory\n");
            return;
        }
        bootlog("OK\n");
        closedir(mountDir);

        // Try load configuration if requested
        if (loadConfig)
        {
            this->loadConfig();
        }

        bootlog("Initializing Bootloader Manager ... ");

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
                    kernelFiles.push_back(kfile);
                }
            }
            closedir(dir);

            if (kernelFiles.empty())
                throw new std::runtime_error("No kernel files found in directory");

            valid = true;
        }
        catch (const std::exception &e)
        {
            bootlog("KO : %s\n", e.what());
            return;
        }

        bootlog("OK : %u kernel files\n", kernelFiles.size());
    }

    size_t BootloaderManager::getFileSize(const std::string &filepath)
    {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0)
            return st.st_size;
        return static_cast<size_t>(-1);
    }

    BootloaderManager &BootloaderManager::selectFile()
    {
        // Validity barrier
        if (!valid)
        {
            bootlog("Skip selection, bootloader manager not valid");
            return *this;
        }

        // Check if a default or alternative file have been selected
        std::string t = (DefaultFile != "" ? DefaultFile : AlternativeFile);
        if (t != "")
        {
            for (auto file : kernelFiles)
            {
                if (t == file->getFilename())
                {
                    selectedFile = file;
                    bootlog("Config selected kernel file: %s\n", selectedFile->getFilename().c_str());
                    return *this;
                }
            }
        }

        // Rollback on user choice
        printf("Available kernel files:\n");
        int i = 0;
        for (auto file : kernelFiles)
        {
            printf(" %d) %s\n", i++, file->getFilename().c_str());
        }
        size_t selected = -1;
        while (selected < 0 || selected >= kernelFiles.size())
        {
            printf("Select an index: ");
            fflush(stdout);
            scanf("%u", &selected);
        }

        selectedFile = kernelFiles[selected];

        if (selectedFile != nullptr)
            bootlog("User selected kernel file: %s\n", selectedFile->getFilename().c_str());
        else
            printf("Unwanted error : file selected is null\n");

        return *this;
    }

    BootloaderManager &BootloaderManager::loadConfig()
    {
        bootlog("Loading configuration ... ");

        // Load configuration from the config.txt file in the mountpoint
        std::string configPath = mountpoint + "/config.txt";

        FILE *configFile = fopen(configPath.c_str(), "r");
        if (!configFile)
        {
            bootlog("KO continuing with defaults\n");
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

        bootlog("OK\n");
        return *this;
    }

    BootloaderManager &BootloaderManager::loadSelectedFile()
    {
        // Validity barrier
        if (!valid)
        {
            bootlog("Skip loading, bootloader manager not valid\n");
            return *this;
        }

        if (selectedFile == nullptr)
        {
            bootlog("Skip loading, no kernel file selected\n");
            return *this;
        }

        bootlog("Loading selected kernel file ... ");

        kernelFileStart = nullptr;
        kernelFileEnd = nullptr;

        // Load the selected kernel file into memory
        selectedFile->load(&kernelFileStart, &kernelFileEnd);
        if (kernelFileStart == nullptr || kernelFileEnd == nullptr)
        {
            kernelFileStart = nullptr;
            kernelFileEnd = nullptr;
        }

        if (kernelFileStart == nullptr)
            bootlog("KO loading kernel file\n");
        else
            bootlog("OK loaded from %p to %p.\n", kernelFileStart, kernelFileEnd);

        return *this;
    }

    void BootloaderManager::boot()
    {
        // Validity barrier
        if (!valid)
        {
            bootlog("Skip booting, bootloader manager not valid\n");
            return;
        }

        if (kernelFileStart == nullptr || kernelFileEnd == nullptr)
        {
            bootlog("Kernel file not loaded, cannot boot\n");
            return;
        }

        bootlog("! Booting kernel file\n");

        GlobalIrqLock lock;
        bootlog("! GlobalLock acquired\n");

        // FilesystemManager::instance().umountAll(); // Does not work, get stuck
        FilesystemManager::instance().umount("/sd");
        FilesystemManager::instance().umount("/dev");
        FilesystemManager::instance().umount("/");
        bootlog("! Unmounted all filesystem correctly\n");

        size_t resetHandlerDisplacement = 0x4;
        unsigned int *resetHandlerAddressPointer = (unsigned int*)((unsigned int)kernelFileStart + resetHandlerDisplacement);
        bootlog("! Reset handler address pointer at %p\n", (void *)resetHandlerAddressPointer);

        unsigned int resetHandlerAddress = *resetHandlerAddressPointer;
        bootlog("! Reset handler address value: %08x\n", resetHandlerAddress);

        unsigned int relativeResetHandlerAddress = resetHandlerAddress - SRAM_BASE + (unsigned int)kernelFileStart;
        bootlog("! Relative reset handler address: %08x\n", relativeResetHandlerAddress);

        void *resetHandlerAddressPtr = (void *)relativeResetHandlerAddress;
        bootlog("! Will call reset handler at: %p\n", resetHandlerAddressPtr);

        __asm__ __volatile__(
            "cpsid i              \n\t" // Disable interrupts
            "mov r0, %[str]       \n\t" // Load kernelFileStart address into r0
            "mov r1, %[end]     \n\t" // Load the Address of resetHandler into r1
            "mov r2, %[rst]       \n\t" // Load kernelFileEnd address into r1
            "bx r2                \n\t" // Branch to the reset handler
            : : [str] "r"(kernelFileStart),
                [end] "r"(kernelFileEnd),
                [rst] "r"(resetHandlerAddressPtr) :);

        // This point should never be reached

        bootlog("KERNEL BOOT FAILED\n");
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
        CHECK_TAG(tag, "default", DefaultFile, value)
        CHECK_TAG(tag, "alternative", AlternativeFile, value)
        CHECK_TAG(tag, "verbose", Verbose, true) //(value == "1")) TODO remove

#undef CHECK_TAG
    }
}

#endif // AS_BOOTLOADER