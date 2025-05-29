
#include <cstdio>
#include "miosix.h"

#include "as_bootloader/bl_manager.h"

using namespace std;
using namespace miosix;

int main()
{
    printf("========= Bootloader Started =========\n");

    BootloaderManager blManager;
    blManager.selectFile().loadSelectedFile().boot();

    // This point should never be reached
    printf("===== Bootloader Exiting : FAIL ======\n");
    exit(EXIT_FAILURE);
}
