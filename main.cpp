
#include <cstdio>
#include <iostream>
#include "miosix.h"

#include "bl_manager.h"

using namespace std;
using namespace miosix;

int main()
{
    BootloaderManager blManager;

    if (!blManager.isFileSelected()) // If there is no already selected file
    {
        size_t selectedFile = 0;
        std::vector<std::string> kernelFiles = blManager.getKernelFiles();

        // Rollback on user choice
        iprintf("Available kernel files in /%s:\n", blManager.getTkernelsDir().c_str());
        for (size_t i = 0; i < kernelFiles.size(); ++i)
        {
            iprintf("%d) %s\n", i, kernelFiles[i].c_str());
        }
        do
        {
            iprintf("Select an index: ");
            fflush(stdout);
            iscanf("%u", &selectedFile);

            // Prevents invalid input from causing an infinite loop
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (blManager.selectFile(selectedFile) == false);
    }

    blManager.boot();

    // This point should never be reached
    exit(EXIT_FAILURE);
}
