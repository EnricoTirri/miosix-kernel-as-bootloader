// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#include "bl_manager.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace miosix
{
    BootloaderManager::BootloaderManager(const std::string &mountpoint, const std::string &kernelsDir)
        : mountpoint(mountpoint), kernelsDir(kernelsDir), valid(false)
    {

        std::string filesDir = mountpoint + "/" + kernelsDir;

        DIR *dir = opendir(filesDir.c_str());
        if (!dir)
        {
            printf("Mountpoint does not exist or is not a directory: %s\n", filesDir.c_str());
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
            printf("Select an index: ");
            fflush(stdout);
            scanf("%u", &selected);
        }

        return kernelFiles.at(selected);
    }

    void BootloaderManager::loadConfig()
    {
        std::string configPath = mountpoint + "/config.txt";

        FILE *configFile = fopen(configPath.c_str(), "r");
        if (!configFile)
        {
            printf("Configuration file not found at %s\n", configPath.c_str());
            return;
        }

        printf("Reading configuration from %s...\n", configPath.c_str());

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), configFile))
        {
            // Remove trailing newline character
            buffer[strcspn(buffer, "\n")] = '\0';

            // Find the delimiter ':' in the line
            char *delimiter = strchr(buffer, ':');
            if (delimiter == nullptr)
            {
                printf("Invalid config line (missing ':'): %s\n", buffer);
                continue;
            }

            // Split the line into tag and value
            *delimiter = '\0'; // Replace ':' with null terminator
            std::string tag = buffer;
            std::string value = delimiter + 1;

            assignTag(tag, value);
        }

        fclose(configFile);
        printf("Configuration loaded successfully.\n");
    }

#define CHECK_TAG(tagVar, tagVal, valueDst, valueSrc) \
    {                                                 \
        if (tagVar == tagVal)                         \
        {                                             \
            valueDst = valueSrc;                      \
            return;                                   \
        }                                             \
    }

    void BootloaderManager::assignTag(const std::string &tag, const std::string &value)
    {
        CHECK_TAG(tag, "default", DefaultFile, value)
        CHECK_TAG(tag, "alternative", AlternativeFile, value)
        CHECK_TAG(tag, "verbose", Verbose, (value == "1"))
        printf("Unknown tag: %s\n", tag.c_str());
    }

#undef CHECK_TAG

}