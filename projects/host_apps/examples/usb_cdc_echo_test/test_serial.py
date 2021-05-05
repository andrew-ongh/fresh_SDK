#!/usr/bin/env python
import sys
import serial
import time
import signal
def error(errstr):
    print "Error: ", errstr
    exit(1)

spin = '|/-\\|/-\\'
spin_st = 0
spin_delay = 0
SPIN_MAX_DELAY = 16
total_count = 0
start_time = 0
error_count = 0

def show_spinner():
    global spin_st, spin_delay
    spin_delay += 1
    if spin_delay == SPIN_MAX_DELAY:
	spin_delay = 0
    	print "\r%c" % spin[spin_st],
	sys.stdout.flush()
	spin_st = (spin_st + 1) % len(spin)

def show_stats(prev, c):
    global total_count
    if (time.time() - prev ) > 1:
        bw = c / (time.time() - prev)
        print "Speed: %d Bytes/sec, error bytes: %d" % (bw, error_count),
        total_count += c
        return time.time(), 0

    return prev, c

def signal_handler(signal, frame):
    ttime = time.time() - start_time
    bw = total_count / ttime
    print
    print
    print "Stats: (Total Bytes / Error bytes / Time (s) / Avg speed (Bps)): " \
        " %d, %d, %d, %d" % \
        (total_count, error_count, ttime, bw)
    exit(0)

def get_str_diff(s1, s2):
    c = 0
    for i in range(len(s1)):
        if s1[i] != s2[i]:
            c += 1

    return c          

def test_cdc(port):
    global start_time
    global error_count
    signal.signal(signal.SIGINT, signal_handler)
    s = "".join([ '%02x' % i for i in range(256) ]).encode('utf-8')
    l = len(s)
    print len(s), s
    ser = serial.Serial(timeout = 2)
    ser.baudrate = 115200
    ser.port = port
    ser.open()
    prev = time.time()
    start_time = prev
    c = 0
    while True:
        show_spinner()
        r = ser.write(s)
        if r != l:
            error("Written %d instead of %d bytes" % (r, l))
        show_spinner()
        d = ser.read(l)
        if d != s:
            if len(d) != l:
                error("Received %d instead of %d bytes" % (len(d), l))
            else:
                error_count += get_str_diff(s, d)
        c += 2 * l
        prev, c = show_stats(prev, c)
    

def main():
    if len(sys.argv) < 2:
        print "Usage: %s <serial port>" % sys.argv[0]
	exit(1)

    test_cdc(sys.argv[1])

if __name__ == "__main__":
    main()
