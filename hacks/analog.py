#!/usr/bin/python

import sys
import time
import argparse
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('log_file')
parser.add_argument('-q', '--queue', help='plot queue size', action='store_true')
parser.add_argument('-e', '--encoder-threads', help='plot encoder thread activity', action='store_true')
parser.add_argument('-f', '--plot-first-encoder', help='plot more detailed activity of the first encoder thread', action='store_true')
parser.add_argument('--dump-first-encoder', help='dump activity of the first encoder thread', action='store_true')
parser.add_argument('--from', help='time in seconds to start at', type=int, dest='from_time')
parser.add_argument('--to', help='time in seconds to stop at', type=int, dest='to_time')
args = parser.parse_args()

def find_nth(haystack, needle, n):
    start = haystack.find(needle)
    while start >= 0 and n > 1:
        start = haystack.find(needle, start+len(needle))
        n -= 1
    return start

class Time:
    def __init__(self, s, m = 0):
        self.seconds = s
        self.microseconds = m

    def __str__(self):
        return '%d:%d' % (self.seconds, self.microseconds)

    def float_seconds(self):
        return self.seconds + self.microseconds / 1000000.0

    def __sub__(self, x):
        m = self.microseconds - x.microseconds
        if m < 0:
            return Time(self.seconds - x.seconds - 1, m + 1000000)
        else:
            return Time(self.seconds - x.seconds, m)

queue_size = []
encoder_thread_events = dict()

def add_encoder_thread_event(thread, time, event):
    global encoder_thread_events
    if thread in encoder_thread_events:
        encoder_thread_events[thread].append((time, event))
    else:
        encoder_thread_events[thread] = [(time, event)]

f = open(args.log_file)
start = None
while True:
    l = f.readline()
    if l == '':
        break

    l = l.strip()
    p = l.split()

    if len(p) == 0:
        continue

    if len(p[0].split(':')) == 2:
        # s:us timestamp
        x = p[0].split(':')
        T = Time(int(x[0]), int(x[1]))
        message = l[l.find(' ')+1:]
    else:
        # Date/time timestamp
        s = find_nth(l, ':', 3)
        T = Time(time.mktime(time.strptime(l[:s])))
        message = l[s+2:]

    if start is None:
        start = T
    else:
        T = T - start

    thread = None
    if message.startswith('['):
        thread = message.split()[0][1:-1]
        message = message[message.find(' ')+1:]

    if message.startswith('adding to queue of '):
        queue_size.append((T, int(message.split()[4])))
    elif message.startswith('encoder thread sleeps'):
        add_encoder_thread_event(thread, T, 'sleep')
    elif message.startswith('encoder thread wakes'):
        add_encoder_thread_event(thread, T, 'wake')
    elif message.startswith('encoder thread begins local encode'):
        add_encoder_thread_event(thread, T, 'begin_encode')
    elif message.startswith('MagickImageProxy begins read and decode'):
        add_encoder_thread_event(thread, T, 'magick_begin_decode')
    elif message.startswith('MagickImageProxy completes read and decode'):
        add_encoder_thread_event(thread, T, 'magick_end_decode')
    elif message.startswith('encoder thread finishes local encode'):
        add_encoder_thread_event(thread, T, 'end_encode')

if args.queue:
    plt.figure()
    x = []
    y = []
    for q in queue_size:
        x.append(q[0].seconds)
        y.append(q[1])

    plt.plot(x, y)
    plt.show()
elif args.encoder_threads:
    plt.figure()
    N = len(encoder_thread_events)
    n = 1
    for thread, events in encoder_thread_events.iteritems():
        plt.subplot(N, 1, n)
        x = []
        y = []
        previous = 0
        for e in events:
            if args.from_time is not None and e[0].float_seconds() <= args.from_time:
                continue
            if args.to_time is not None and e[0].float_seconds() >= args.to_time:
                continue
            x.append(e[0].float_seconds())
            x.append(e[0].float_seconds())
            y.append(previous)
            if e[1] == 'sleep':
                y.append(0)
            elif e[1] == 'wake':
                y.append(1)
            elif e[1] == 'begin_encode':
                y.append(2)
            elif e[1] == 'end_encode':
                y.append(1)
            elif e[1] == 'magick_begin_decode':
                y.append(3)
            elif e[1] == 'magick_end_decode':
                y.append(2)

            previous = y[-1]

        plt.plot(x, y)
        n += 1
        break

    plt.show()
elif args.plot_first_encoder:
    plt.figure()
    N = len(encoder_thread_events)
    n = 1
    events = encoder_thread_events.itervalues().next()

    N = 6
    n = 1
    for t in ['sleep', 'wake', 'begin_encode', 'magick_begin_decode', 'magick_end_decode', 'end_encode']:
        plt.subplot(N, 1, n)
        x = []
        y = []
        for e in events:
            if args.from_time is not None and e[0].float_seconds() <= args.from_time:
                continue
            if args.to_time is not None and e[0].float_seconds() >= args.to_time:
                continue
            if e[1] == t:
                x.append(e[0].float_seconds())
                x.append(e[0].float_seconds())
                x.append(e[0].float_seconds())
                y.append(0)
                y.append(1)
                y.append(0)
                
        plt.plot(x, y)
        plt.title(t)
        n += 1

    plt.show()
elif args.dump_first_encoder:
    events = encoder_thread_events.itervalues().next()
    last = 0
    for e in events:
        print e[0].float_seconds(), (e[0].float_seconds() - last), e[1]
        last = e[0].float_seconds()
