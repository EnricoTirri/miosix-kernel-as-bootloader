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

        void load(void **kernelFileStart, void **kernelFileEnd)
        {
            *kernelFileStart = nullptr;
            *kernelFileEnd = nullptr;

            void *dest = malloc(filesize);
            if (dest == nullptr)
            {
                throw new std::runtime_error("Failed to allocate memory for kernel file");
            }

            // open file at directory/filename
            std::string fullPath = directory + "/" + filename;
            FILE *file = fopen(fullPath.c_str(), "rb");
            if (file == nullptr)
            {
                free(dest);
                throw new std::runtime_error("Failed to open kernel file: " + fullPath);
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
                        break; // End of file reached
                    }
                    else
                    {
                        free(dest);

                        if (ferror(file))
                        {
                            throw new std::runtime_error("Error reading kernel file: " + std::string(strerror(errno)));
                        }
                        else
                        {
                            throw new std::runtime_error("Unknown error reading kernel file");
                        }
                    }
                }
                totalBytesRead += bytesRead;
            }
            fclose(file);

            if (totalBytesRead != filesize)
            {
                free(dest);
                throw new std::runtime_error("Error loading kernel file: got " + std::to_string(totalBytesRead) + "/" + std::to_string(filesize) + " B");
            }

            *kernelFileStart = dest;
            *kernelFileEnd = (void *)((unsigned int)dest + totalBytesRead);
        }
    };
}