VPATH=\
    src/:\
    src/tools

INC=\
    -Iinclude \

CC=gcc
CFLAGS += $(INC) -Werror -ggdb

arfcn2freq: arfcn2freq.c gsm-band.c
	$(CC) $(CFLAGS) $^ -o $@

earfcn2ul: earfcn2ul.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

earfcn2dl: earfcn2dl.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

fft-scan: fft-scan.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@ -lbingewatch -lSoapySDR -lpthread -lradpool -lliquid -lfftw3f -lm

lte-info: lte-info.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@
