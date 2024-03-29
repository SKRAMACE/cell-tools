VPATH=\
    src/:\
    src/tools

INC=\
    -Iinclude \

CC=gcc
CFLAGS += $(INC) -Werror -ggdb

BINGEWATCH=\
    -lbingewatch_soapy \
    -lbingewatch_uhd \
    -lbingewatch \
	/usr/local/lib/libSoapySDR.so \
    -luhd \

.IGNORE: clean
.PHONY: clean

arfcn2dl: arfcn2dl.c gsm-band.c
	$(CC) $(CFLAGS) $^ -o $@

earfcn2ul: earfcn2ul.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

earfcn2band: earfcn2band.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

earfcn2dl: earfcn2dl.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

freq2earfcn: freq2earfcn.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

fft-scan-band: fft-scan-band.c fft-scan.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@ $(BINGEWATCH) -lpthread -lmemex -lliquid -lfftw3f -lm

fft-scan-freq: fft-scan-freq.c fft-scan.c
	$(CC) $(CFLAGS) $^ -o $@ $(BINGEWATCH) -lSoapySDR -lpthread -lmemex -lliquid -lfftw3f -lm

lte-info: lte-info.c lte-band.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.obj
	rm -f arfcn2dl earfcn2ul earfcn2dl earfcn2band freq2earfcn fft-scan-band fft-scan-freq lte-info
