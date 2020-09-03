#include <stdlib.h>
#include <stdio.h>

#include "gsm-band.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("usage: %s ARFCN [BAND]\n", argv[0]);
        printf("  Bands:\n");
        int b = 0;
        while (b < GSM_EOL) {
            printf("   %d: %s\n", b++, GSM_BAND_STR(b)); 
        }
        printf("\n");
        exit(1);
    }

    int arfcn = atoi(argv[1]);

    double freq = 0.0;
    if (argc > 2) {
        int band = atoi(argv[2]);
        band_arfcn_to_dl(band, arfcn, &freq);
    } else {
        arfcn_to_dl(arfcn, &freq);
    }
    printf("%0.1f\n", freq);
    exit(0);
}
