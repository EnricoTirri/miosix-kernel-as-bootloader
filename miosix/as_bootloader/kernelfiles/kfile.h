//TODO must edit: LICENSE

//TODO must edit: #ifdef WITH_FATFS

#pragma once

#include <iostream>
#include <string>

namespace miosix
{

    class KernelFile
    {
    protected:
        std::string directory;
        std::string filename;
        size_t filesize; // size in bytes

    public:
        KernelFile(const std::string &dir, const std::string &fn, size_t size)
            : directory(dir), filename(fn), filesize(size) {}

        virtual ~KernelFile() {}

        const std::string &getDirectory() const { return directory; }

        const std::string &getFilename() const { return filename; }

        size_t getSize() const { return filesize; }

        virtual void load(void **kernelFileStart, void **kernelFileEnd)
        {
            *kernelFileStart = nullptr;
        }
    };
} // namespace miosix


// Ensure registration of kernel file classes
#include "kfileregister.h"