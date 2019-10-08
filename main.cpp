#include "src/fstore.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    FStore fstore;
    if (!fstore.Init("/tmp/m", 4096*1000, 100)) {
        return 1;
    }

    if (!fstore.PartitionFiles()) {
        return 1;
    }

    return 0;
}
