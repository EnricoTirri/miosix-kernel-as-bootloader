
#include <cstdio>
#include "miosix.h"

#include "as_bootloader/bl_manager.h"
#include "filesystem/file_access.h"

using namespace std;
using namespace miosix;

void exit_bl()
{
    printf("===== Bootloader Exiting : FAIL ======\n");
    exit(EXIT_FAILURE);
}

int main()
{
    printf("========= Bootloader Started =========\n");

    BootloaderManager blManager;
    blManager.selectFile().loadSelectedFile().boot();

    exit_bl(); // This point should never be reached
}
