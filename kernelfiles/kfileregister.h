// TODO must edit: LICENSE

#pragma once

#include "kfilefactory.h"
#include "kfile.h"

#define REISTER_KERNELFILE_CLASS(EXT, CLASS)                                                                \
    namespace miosix                                                                                        \
    {                                                                                                       \
        struct CLASS##Register                                                                              \
        {                                                                                                   \
            CLASS##Register()                                                                               \
            {                                                                                               \
                KernelFileFactory::instance().registerBuilder(                                              \
                    EXT,                                                                                    \
                    [](const std::string &dir,                                                              \
                       const std::string &fn, size_t size)                                                  \
                        -> std::unique_ptr<KernelFile> { return std::make_unique<CLASS>(dir, fn, size); }); \
            }                                                                                               \
        };                                                                                                  \
        static CLASS##Register global_##CLASS##_register;                                                   \
    }

#include "bin_kfile.h"
REISTER_KERNELFILE_CLASS(".bin", BinKernelFile)
