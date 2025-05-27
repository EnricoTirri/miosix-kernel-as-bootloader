// TODO must edit: LICENSE

// TODO must edit: #ifdef WITH_FATFS

#pragma once

#include "kfile.h"

namespace miosix
{
    class BinKernelFile : public KernelFile
    {
    public:
        BinKernelFile(const std::string &dir, const std::string &fn, size_t size)
            : KernelFile(dir, fn, size) {}

        void load(void **kernelFileStart, void **kernelFileEnd) override
        {
            void *dest = malloc(filesize);
            if (dest == nullptr)
            {
                printf("Failed to allocate memory for kernel file: %s\n", filename.c_str());

                *kernelFileStart = nullptr;
                return;
            }

            // open file at directory/filename
            std::string fullPath = directory + "/" + filename;
            FILE *file = fopen(fullPath.c_str(), "rb");
            if (file == nullptr)
            {
                printf("Failed to open kernel file: %s\n", fullPath.c_str());
                free(dest);

                *kernelFileStart = nullptr;
                return;
            }

            // read file content into dest
            size_t chunkSize = 1024;
            size_t totalBytesRead = 0;

            size_t bytesRead = fread(dest, 1, chunkSize, file);
            while (bytesRead > 0)
            {
                bytesRead = fread((void *)((unsigned int)dest + totalBytesRead), 1, chunkSize, file);
                totalBytesRead += bytesRead;
            }
            fclose(file);

            if (totalBytesRead != filesize)
            {
                printf("Unable to reaad all file content: expected %u bytes, got %u bytes\n", filesize, totalBytesRead);
                free(dest);

                kernelFileStart = nullptr;
                return;
            }

            *kernelFileStart = dest;
            *kernelFileEnd = (void *)((unsigned int)dest + totalBytesRead);
        }
    };
}