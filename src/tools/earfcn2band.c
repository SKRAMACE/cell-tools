#include <stdlib.h>
#include <stdio.h>

#include "lte-band.h"

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s EARFCN\n", argv[0]);
        printf("\n");
        exit(1);
    }

    int earfcn = atoi(argv[1]);

    int band = earfcn_to_band(earfcn);
    if (band > 0) {
        printf("%d\n", band);
        exit(0);
    }
    exit(1);
}
