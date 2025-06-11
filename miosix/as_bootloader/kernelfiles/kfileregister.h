//TODO must edit: LICENSE

//TODO must edit: #ifdef WITH_FATFS

#pragma once

#include "kfilefactory.h"
#include "kfile.h"

#define REISTER_KERNELFILE_CLASS(EXT, CLASS)                                                                \
    namespace miosix                                                                                            \
    {                                                                                                       \
        struct CLASS##Register                                                                              \
        {                                                                                                   \
            CLASS##Register()                                                                               \
            {                                                                                               \
                KernelFileFactory::instance().registerBuilder(                                              \
                    EXT,                                                                                    \
                    [](const std::string &dir,                                                              \
                       const std::string &fn, size_t size)                                                  \
                        -> std::shared_ptr<KernelFile> { return std::make_shared<CLASS>(dir, fn, size); }); \
            }                                                                                               \
        };                                                                                                  \
        static CLASS##Register global_##CLASS##_register;                                                   \
    }


#include "bin_kfile.h"
REISTER_KERNELFILE_CLASS(".bin", BinKernelFile)
