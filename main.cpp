
#include <cstdio>
#include "miosix.h"

#include "bl_manager.h"

using namespace std;
using namespace miosix;

int main()
{
    BootloaderManager blManager;

    blManager.selectFile().boot();

    // This point should never be reached
    exit(EXIT_FAILURE);
}
