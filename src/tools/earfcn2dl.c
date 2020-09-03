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

    double freq = 0.0;
    if (earfcn_to_dl(earfcn, &freq) == 0) {
        printf("%0.1f\n", freq);
        exit(0);
    }
    exit(1);
}
