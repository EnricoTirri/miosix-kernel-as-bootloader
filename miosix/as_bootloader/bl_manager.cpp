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
            bootlog("Skip selection, bootloader manager not valid\n");
            return *this;
        }

        selectedFile = nullptr;

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
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Prevents reading leftover characters
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

        relocationAddress = nullptr;
        kernelFileStart = nullptr;
        kernelFileEnd = nullptr;

        // Load the selected kernel file into memory
        try
        {
            selectedFile->load(&relocationAddress, &kernelFileStart, &kernelFileEnd);
        }
        catch (const std::exception &e)
        {
            bootlog("KO : %s\n", e.what());
            relocationAddress = nullptr;
            kernelFileStart = nullptr;
            kernelFileEnd = nullptr;
            return *this;
        }

        bootlog("OK\n\t - Loaded from %p to %p.\n\t - Relocation at %p\n", kernelFileStart, kernelFileEnd, relocationAddress);

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

        if (kernelFileStart == nullptr || kernelFileEnd == nullptr || relocationAddress == nullptr)
        {
            bootlog("Kernel file not loaded, cannot boot\n");
            return;
        }

        bootlog("! Booting kernel file\n");

        //GlobalIrqLock lock;
        bootlog("! GlobalLock acquired\n");

        FilesystemManager::instance().umount("/sd");
        FilesystemManager::instance().umount("/dev");
        FilesystemManager::instance().umount("/");
        bootlog("! Unmounted all filesystem correctly\n");

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