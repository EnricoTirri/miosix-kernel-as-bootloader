#include <cstdio>
#include <iostream>
#include "miosix.h"

#include "bl_manager.h"

using namespace std;
using namespace miosix;


#define VERSION "1"


void clearInput()
{
    std::cin.clear();                                                   // Clear any error flags
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard the rest of the line
}

void changeMountPointRoutine(BootloaderManager &blManager)
{
    do
    {
        char buffer[256];

        iprintf("Enter new mountpoint (default: /sd/): ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin))
        {
            // Remove newline if present
            buffer[strcspn(buffer, "\n")] = '\0';

            std::string mountpoint = strlen(buffer) > 0 ? buffer : "/sd/";
            blManager.setMountpoint(mountpoint, true);
        }
        else
        {
            blManager.setMountpoint("/sd/", true);
        }

    } while (blManager.getKernelFiles().empty());
}

void selectKernelFileRoutine(BootloaderManager &blManager)
{
    size_t selectedFile = 0;

    std::vector<std::string> kernelFiles = blManager.getKernelFiles();

    iprintf("Available kernel files in /%s:\n", blManager.getTkernelsDir().c_str());
    for (size_t i = 0; i < kernelFiles.size(); ++i)
    {
        iprintf("%d) %s\n", i + 1, kernelFiles[i].c_str());
    }
    iprintf("+ Enter 0 to change mountpoint\n");

    do
    {
        iprintf("Select an index: ");
        fflush(stdout);
        iscanf("%u", &selectedFile);

        // Prevents invalid input from causing an infinite loop
        clearInput();

        if (selectedFile == 0)
            break;
    } while (blManager.selectFile(selectedFile - 1) == false);
}

int main()
{
    iprintf("Miosix SD Card Bootloader V%s\n", VERSION);
    BootloaderManager blManager;

    // If autorun available immediately load the selected kernel file
    if (blManager.isFileSelected())
    {
        iprintf("Autorun detected, running in ");
        fflush(stdout);
        for (int i = 3; i > 0; --i)
        {
            iprintf("%d ", i);
            fflush(stdout);
            Thread::sleep(1000);
        }
        iprintf("\n");
        blManager.boot();
        exit(EXIT_FAILURE);
    }

    while (blManager.getKernelFiles().empty())
    {
        changeMountPointRoutine(blManager);
    }

    do
    {
        selectKernelFileRoutine(blManager);
        if(!blManager.isFileSelected()){
            changeMountPointRoutine(blManager);
        }
    } while (!blManager.isFileSelected());

    blManager.boot();

    // This point should never be reached
    exit(EXIT_FAILURE);
}
