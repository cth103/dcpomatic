#!/usr/bin/python
#
# Analyse a DCP-o-matic log file to extract various information.
#

import sys
import time
import argparse
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('log_file')
parser.add_argument('-q', '--queue', help='plot queue size', action='store_true')
parser.add_argument('-e', '--encoder-threads', help='plot encoder thread activity', action='store_true')
parser.add_argument('-f', '--plot-first-encoder', help='plot more detailed activity of the first encoder thread', action='store_true')
parser.add_argument('-s', '--fps-stats', help='frames-per-second stats', action='store_true')
parser.add_argument('--encoder-stats', help='encoder thread activity stats', action='store_true')
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

# Representation of time in seconds and microseconds
class Time:
    def __init__(self, s = 0, m = 0):
        self.seconds = s
        self.microseconds = m

    def __str__(self):
        return '%d:%d' % (self.seconds, self.microseconds)

    def float_seconds(self):
        return self.seconds + self.microseconds / 1000000.0

    def __iadd__(self, x):
        self.microseconds += x.microseconds
        self.seconds += x.seconds
        if self.microseconds >= 1000000:
            self.microseconds -= 1000000
            self.seconds += 1
        return self

    def __sub__(self, x):
        m = self.microseconds - x.microseconds
        if m < 0:
            return Time(self.seconds - x.seconds - 1, m + 1000000)
        else:
            return Time(self.seconds - x.seconds, m)

queue_size = []
general_events = []
encoder_threads = []
encoder_thread_events = dict()

def add_encoder_thread_event(thread, time, event):
    global encoder_threads
    global encoder_thread_events
    if not thread in encoder_threads:
        encoder_threads.append(thread)
        encoder_thread_events[thread] = [(time, event)]
    else:
        encoder_thread_events[thread].append((time, event))

def add_general_event(time, event):
    global general_events
    general_events.append((time, event))

f = open(args.log_file)
start = None
while True:
    l = f.readline()
    if l == '':
        break

    p = l.strip().split()

    if len(p) == 0:
        continue

    if len(p[0].split(':')) == 2:
        # s:us timestamp: LOG_TIMING
        t = p[0].split(':')
        T = Time(int(t[0]), int(t[1]))
        p = l.split()
        message = p[1]
        values = {}
        for i in range(2, len(p)):
            x = p[i].split('=')
            values[x[0]] = x[1]
    else:
        # Date/time timestamp: other LOG_*
        s = find_nth(l, ':', 3)
        T = Time(time.mktime(time.strptime(l[:s])))
        message = l[s+2:]

    # T is elapsed time since the first log message
    if start is None:
        start = T
    else:
        T = T - start

    # Not-so-human-readable log messages (LOG_TIMING)
    if message == 'add-frame-to-queue':
        queue_size.append((T, values['queue']))
    elif message in ['encoder-sleep', 'encoder-wake', 'start-local-encode', 'finish-local-encode', 'start-remote-send', 'finish-remote-send', 'start-remote-encode-and-receive', 'finish-remote-encode-and-receive']:
        add_encoder_thread_event(values['thread'], T, message)
    # Human-readable log message (other LOG_*)
    elif message.startswith('Finished locally-encoded'):
        add_general_event(T, 'end_local_encode')
    elif message.startswith('Finished remotely-encoded'):
        add_general_event(T, 'end_remote_encode')
    elif message.startswith('Transcode job starting'):
        add_general_event(T, 'begin_transcode')
    elif message.startswith('Transcode job completed successfully'):
        add_general_event(T, 'end_transcode')

if args.queue:
    # Plot queue size against time; queue_size contains times and queue sizes
    plt.figure()
    x = []
    y = []
    for q in queue_size:
        x.append(q[0].seconds)
        y.append(q[1])

    plt.plot(x, y)
    plt.show()

elif args.encoder_threads:
    # Plot the things that are happening in each encoder thread with time
    # y=0 thread is sleeping
    # y=1 thread is awake
    # y=2 thread is encoding
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

            previous = y[-1]

        plt.plot(x, y)
        n += 1

    plt.show()

elif args.plot_first_encoder:
    plt.figure()
    N = len(encoder_thread_events)
    n = 1
    events = encoder_thread_events.itervalues().next()

    N = 6
    n = 1
    for t in ['sleep', 'wake', 'begin_encode', 'end_encode']:
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

elif args.fps_stats:
    local = 0
    remote = 0
    start = None
    end = None
    for e in general_events:
        if e[1] == 'begin_transcode':
            start = e[0]
        elif e[1] == 'end_transcode':
            end = e[0]
        elif e[1] == 'end_local_encode':
            local += 1
        elif e[1] == 'end_remote_encode':
            remote += 1

    if end == None:
        print 'Job did not appear to end'
        sys.exit(1)

    duration = end - start

    print 'Job ran for %fs' % duration.float_seconds()
    print '%d local and %d remote' % (local, remote)
    print '%.2f fps local and %.2f fps remote' % (local / duration.float_seconds(), remote / duration.float_seconds())

elif args.encoder_stats:
    # Broad stats on what encoder threads spent their time doing
    for t in encoder_threads:
        last = None
        asleep = Time()
        encoding = Time()
        wakes = 0
        for e in encoder_thread_events[t]:
            if e[1] == 'encoder-sleep':
                if last is not None:
                    encoding += e[0] - last
                last = e[0]
            elif e[1] == 'encoder-wake':
                wakes += 1
                asleep += e[0] - last
                last = e[0]

        print '-- Encoder thread %s' % t
        print 'Awoken %d times' % wakes
        total = asleep.float_seconds() + encoding.float_seconds()
        print 'Asleep: %s (%s%%)' % (asleep, asleep.float_seconds() * 100 / total)
        print 'Encoding: %s (%s%%)' % (encoding, encoding.float_seconds() * 100 / total)
