#!/usr/bin/env python

import os
import re
import sys
import csv
import stat
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

def movmean(a, n):
    if n <= 0:
        return a

    # Calculate head and tail padding
    n_half = ((n - 1) / 2) + 1
    head_pad = n_half

    if n & 1:
        tail_pad = n_half - 1
    else:
        tail_pad = n_half
    
    # Build padded array
    head = np.zeros(head_pad)
    tail = np.zeros(tail_pad-1)
    aa = np.concatenate((head, a, tail))

    # Calculate moving sum
    ret = np.convolve(aa, np.ones((n,)), mode='valid')

    # Calculate mean scaling
    if n & 1:
        head_scale = np.arange(head_pad - 1, n, 1)
    else:
        head_scale = np.arange(head_pad, n, 1)

    tail_scale = np.arange(n, n-tail_pad, -1)
    mid_scale = n * np.ones(len(a) - n)
    scale = np.concatenate((head_scale, mid_scale, tail_scale));

    # Run movmean operation (scaled convolution)
    ret = np.convolve(aa, np.ones((n,)), mode='valid')

    try:
        ret /= scale
    except ValueError as e:
        print 'ERROR: Array length mismatch:'
        print '\t%d + %d + %d = %d' % (len(head), len(a), len(tail), len(aa))
        print '\t%d + %d + %d = %d' % (len(head_scale), len(mid_scale), len(tail_scale), len(scale))
        print '\tmovmean(%s{%d},%d) = %s{%d}' % (type(aa).__name__, len(aa), n, type(ret).__name__, len(ret))
        print ''
        raise e

    return ret

def to_mhz(x):
    return float(x) / 1000000

class BandInfo(object):
    def __init__(self, band):
        with open('conf/lte-band.csv') as csvfile:
            reader = csv.DictReader(csvfile)
            band_list = filter(lambda row: row['band'] == band, reader)

        if not band_list:
            raise ValueError('Band %d not found', band)
        self.__dict__.update(band_list[0])

class Scan(object):
    def __init__(self, fname, precision='float32'):
        try:
            self.raw = np.fromfile(fname, precision)
        except IOError as e:
            self.raw = np.array((), precision)

        self.smooth_factor = 20
        self.smooth_iter = 3

        self.fname = fname
        self._parse_fname()

    def _parse_fname(self):
        fname = self.fname
        m = re.search(r'band(?P<band>[0-9]+)-binres(?P<res>[0-9]+)', fname)
        if m:
            self.band = m.group('band')
            self.res = m.group('res')
            info = BandInfo(self.band)
            self.freq = info.dl_lo
            self.title = 'Band {band} ({name})'.format(**info.__dict__) + ' res=%s' % self.res
            return

        m = re.search(r'freq(?P<freq>[0-9]+)-bw(?P<bw>[0-9]+)-binres(?P<res>[0-9]+)', fname)
        if m:
            self.freq = m.group('freq')
            print self.freq
            self.bw = m.group('bw')
            print self.bw
            self.res = m.group('res')
            print self.res
            self.title = '%f - %f (res=%s)' % \
                (float(self.freq), float(self.freq) + float(self.bw), self.res)
            return
        return

    def _build_plot_data(self):
        f0 = to_mhz(self.freq)
        step = to_mhz(self.res)

        f1 = f0 + step * len(self.raw)
        f = np.arange(f0, f1, step)

        y = self.raw 
        for i in xrange(self.smooth_iter):
            y = movmean(y, self.smooth_factor)

        if len(f) != len(y):
            ll = min(len(f), len(y))
            f = f[1:ll]
            y = y[1:ll]

        self.f = f
        self.y = y

    def plot(self, ax):
        self._build_plot_data()

        ax.set_yscale("log")
        ax.plot(self.f, self.y)
        ax.set_ylim(min(self.y) / 10, max(self.y) * 10)
        ax.set_title(self.title)

        self.ax = ax

    def __lt__(self, other):
        if float(self.freq) < float(other.freq):
            return True
        return False

def process_dir(dirname):
    files = os.listdir(dirname)
    data = list()
    for f in files:
        path = os.path.join(dirname, f)
        s = Scan(path)
        data.append(s)
    return data

if __name__ == '__main__':
    path = os.environ.get('FFT_SCAN_PATH')
    if not path:
        print 'ERROR: Environment variable "FFT_SCAN_PATH" not set'
        sys.exit(1)
    st = os.stat(path)

    if stat.S_ISDIR(st.st_mode):
        data = process_dir(path)
    elif stat.S_ISREG(st.st_mode):
        data = [Scan(path)]
    else:
        raise ValueError('Invalid input: %s', path)

    fig, ax = plt.subplots(nrows=len(data))
    fig.tight_layout(rect=[0, 0.03, 1, 0.95])

    if len(data) == 1:
        ax = [ax]

    data.sort()
    for i in range(len(ax)):
        data[i].plot(ax[i])

    plt.show()
