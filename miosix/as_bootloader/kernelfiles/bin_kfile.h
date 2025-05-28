// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#pragma once

#include "kfile.h"
#include "util/util.h"
#include "string.h"

namespace miosix
{
    class BinKernelFile : public KernelFile
    {
    public:
        BinKernelFile(const std::string &dir, const std::string &fn, size_t size)
            : KernelFile(dir, fn, size) {}

        void load(void **kernelFileStart, void **kernelFileEnd) override
        {

            *kernelFileStart = nullptr;
            *kernelFileEnd = nullptr;

            void *dest = malloc(filesize);
            if (dest == nullptr)
            {
                printf(" - Failed to allocate memory\n");
                return;
            }

            // open file at directory/filename
            std::string fullPath = directory + "/" + filename;
            FILE *file = fopen(fullPath.c_str(), "rb");
            printf(" + Opening kernel file: %s\n", fullPath.c_str());
            if (file == nullptr)
            {
                printf(" - Failed to open file\n");
                free(dest);
                return;
            }

            // read file content into dest a chunk at a time
            size_t chunkSize = 512; // Sometimes more than 512 bytes at a time is a problem
            size_t totalBytesRead = 0;
            while (totalBytesRead < filesize)
            {
                size_t bytesRead = fread((char *)dest + totalBytesRead, 1, chunkSize, file);
                if (bytesRead == 0)
                {
                    if (feof(file))
                    {
                        printf(" + Reached end of file\n");
                        break; // End of file reached
                    }
                    else
                    {
                        printf(" - Error reading file");

                        if (ferror(file))
                            printf(": %d: %s\n", errno, strerror(errno));
                        else
                            printf("\n");

                        break; // Error reading file
                    }
                }
                totalBytesRead += bytesRead;
            }
            fclose(file);

            if (totalBytesRead != filesize)
            {
                printf(" - Error loading file: got %u/%u B\n", totalBytesRead, filesize);
                free(dest);
                return;
            }

            *kernelFileStart = dest;
            *kernelFileEnd = (void *)((unsigned int)dest + totalBytesRead);
        }
    };
}