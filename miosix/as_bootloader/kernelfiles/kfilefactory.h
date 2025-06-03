//TODO must edit: LICENSE

//TODO must edit: #ifdef WITH_FATFS

#pragma once

#include <map>
#include <functional>
#include <memory>
#include <string>

#include "kfile.h"

#ifdef AS_BOOTLOADER

namespace miosix
{
    class KernelFileFactory
    {
    public:
        using Builder = std::function<std::shared_ptr<KernelFile>(
            const std::string &directory, const std::string &filename, size_t filesize)>;

        static KernelFileFactory &instance()
        {
            static KernelFileFactory factory;
            return factory;
        }

        void registerBuilder(const std::string &ext, Builder builder)
        {
            builders[ext] = builder;
        }

        std::shared_ptr<KernelFile> create(const std::string &directory,
                                           const std::string &filename,
                                           size_t filesize)
        {
            auto ext = getExtension(filename);
            auto it = builders.find(ext);
            if (it != builders.end())
            {
                return it->second(directory, filename, filesize);
            }
            return std::make_unique<KernelFile>(directory, filename, filesize);
        }

    private:
        std::map<std::string, Builder> builders;

        static std::string getExtension(const std::string &filename)
        {
            auto pos = filename.find_last_of('.');
            if (pos != std::string::npos)
            {
                return filename.substr(pos);
            }
            return "";
        }
    };

}

#endif // AS_BOOTLOADER