
#include <cstdio>
#include "miosix.h"

#include "as_bootloader/bl_manager.h"

using namespace std;
using namespace miosix;

void exit_bl()
{
    printf("===== Bootloader Exiting : FAIL ======\n");
    exit(EXIT_FAILURE);
}

inline void copy_and_run(void *destKernelPos, void *kernelFileStart, void *kernelFileEnd)
{
    size_t resetHandlerDisplacement = 0x00000004;

    void *resetHandler = (void *)((unsigned int)destKernelPos + resetHandlerDisplacement);

    __asm__ __volatile__(
        "push {r0-r4}         \n\t"
        "mov r0, %[dst]       \n\t"
        "mov r1, %[src]       \n\t"
        "mov r2, %[end]       \n\t"
        "mov r4, %[run]       \n\t"
        "1:                   \n\t"
        "cmp r1, r2           \n\t"
        "beq 2f               \n\t"
        "ldrb r3, [r1]        \n\t"
        "strb r3, [r0]        \n\t"
        "mov r3, #0           \n\t"
        "strb r3, [r1]        \n\t"
        "add r0, r0, #1       \n\t"
        "add r1, r1, #1       \n\t"
        "b 1b                 \n\t"
        "2:                   \n\t"
        "bx r4                \n\t"
        :
        : [dst] "r"(destKernelPos),
          [src] "r"(kernelFileStart),
          [end] "r"(kernelFileEnd),
          [run] "r"(resetHandler)
        : "memory");
}

int main()
{
    printf("========= Bootloader Started =========\n");

    printf("Initializing Bootloader Manager...\n");
    BootloaderManager blManager("/sd/");

    {
        if (!blManager.isValid())
            exit_bl();

        size_t kernelFilesCount = blManager.getKernelFiles().size();
        if (kernelFilesCount == 0)
        {
            printf("No kernel files found in the bootloader directory.\n");
            exit_bl();
        }
        printf(" + Found %u kernel files.\n", kernelFilesCount);
    }

    std::shared_ptr<KernelFile> selected_kf;

    {
        // TODO selection of kernel file
        selected_kf = blManager.getKernelFiles().at(0);
        printf("Selected kernel file: %s\n", selected_kf->getFilename().c_str());
    }

    void *kernelFileStart;
    void *kernelFileEnd;
    void *destKernelPos;

    {
        printf("Loading kernel file in memory...\n");
        selected_kf->load(&kernelFileStart, &kernelFileEnd);

        if (kernelFileStart == nullptr || kernelFileEnd == nullptr)
        {
            printf("Failed to load kernel file into memory.\n");
            exit_bl();
        }
        printf("Kernel file loaded successfully from %p to %p.\n", kernelFileStart, kernelFileEnd);

        destKernelPos = (void *)SRAM_BASE; // TODO find correctly RAM base address
        printf("Ram base address: %p\n", destKernelPos);
    }

    copy_and_run(destKernelPos, kernelFileStart, kernelFileEnd);

    exit_bl(); // This point should never be reached
}
