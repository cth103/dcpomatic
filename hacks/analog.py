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
parser.add_argument('--encoder-threads', help='plot encoder thread activity', action='store_true')
parser.add_argument('-f', '--plot-first-encoder', help='plot more detailed activity of the first encoder thread', action='store_true')
parser.add_argument('-s', '--fps-stats', help='frames-per-second stats', action='store_true')
parser.add_argument('--encoder-stats', help='encoder thread activity stats', action='store_true')
parser.add_argument('--encoder-dump', help='dump activity of the specified encoder', action='store_true')
parser.add_argument('-e', '--encoder', help='encoder index (from 0)')
parser.add_argument('--from', help='time in seconds to start at', type=int, dest='from_time')
parser.add_argument('--to', help='time in seconds to stop at', type=int, dest='to_time')
parser.add_argument('--max-encoder-threads', help='maximum number of encoder threads to plot with --encoder-threads', type=int, default=None)
args = parser.parse_args()

def find_nth(haystack, needle, n):
    start = haystack.find(needle)
    while start >= 0 and n > 1:
        start = haystack.find(needle, start+len(needle))
        n -= 1
    return start

# Representation of time in seconds and microseconds
class Time:
    def __init__(self, s=0, m=0):
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

class EncoderThread:
    def __init__(self, id):
        self.id = id
        self.events = []
        self.server = None

    def add_event(self, time, message, values):
        self.events.append((time, message, values))

queue_size = []
general_events = []
encoder_threads = []

def find_encoder_thread(id):
    global encoder_threads
    thread = None
    for t in encoder_threads:
        if t.id == id:
            thread = t

    if thread is None:
        thread = EncoderThread(id)
        encoder_threads.append(thread)

    return thread

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
        try:
            T = Time(time.mktime(time.strptime(l[:s])))
        except:
            try:
                T = Time(time.mktime(time.strptime(l[:s], "%d.%m.%Y %H:%M:%S")))
            except:
                try:
                    T = Time(time.mktime(time.strptime(l[:s], "%d/%m/%Y %H:%M:%S")))
                except:
                    x = l[:s]
                    if not x.endswith('M'):
                        x += 'M'
                    T = Time(time.mktime(time.strptime(x, "%d/%m/%Y %H:%M:%S %p")))
        message = l[s+2:]

    # T is elapsed time since the first log message
    if start is None:
        start = T
    else:
        T = T - start

    # Not-so-human-readable log messages (LOG_TIMING)
    if message == 'add-frame-to-queue':
        queue_size.append((T, values['queue']))
    elif message in ['encoder-sleep', 'encoder-wake', 'start-local-encode', 'finish-local-encode', 'start-remote-send', 'start-remote-encode', 'start-remote-receive', 'finish-remote-receive']:
        find_encoder_thread(values['thread']).add_event(T, message, values)
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
    # y=3 thread is awaiting a remote encode

    if args.max_encoder_threads is not None:
        encoder_threads = encoder_threads[0:min(args.max_encoder_threads, len(encoder_threads))]
    
    plt.figure()
    N = len(encoder_threads)
    n = 1
    for thread in encoder_threads:
        plt.subplot(N, 1, n)
        plt.ylim([-0.5, 2.5])
        x = []
        y = []
        previous = 0
        for e in thread.events:
            if args.from_time is not None and e[0].float_seconds() <= args.from_time:
                continue
            if args.to_time is not None and e[0].float_seconds() >= args.to_time:
                continue
            if e[1] == 'start-remote-send' or e[1] == 'finish-remote-send' or e[1] == 'start-remote-receive' or e[1] == 'finish-remote-receive':
                continue
            x.append(e[0].float_seconds())
            x.append(e[0].float_seconds())
            y.append(previous)
            if e[1] == 'encoder-sleep':
                y.append(0)
            elif e[1] == 'encoder-wake':
                y.append(1)
            elif e[1] == 'start-local-encode':
                y.append(2)
            elif e[1] == 'finish-local-encode':
                y.append(1)
            elif e[1] == 'start-remote-encode':
                y.append(3)
            elif e[1] == 'finish-remote-encode':
                y.append(1)
            else:
                print>>sys.stderr,'unknown event %s' % e[1]
                sys.exit(1)

            previous = y[-1]

        plt.plot(x, y)
        n += 1

    plt.show()

elif args.plot_first_encoder:
    plt.figure()
    N = len(encoder_threads)
    n = 1
    events = encoder_threads[0].events

    N = 6
    n = 1
    for t in ['encoder-sleep', 'encoder-wake', 'start-local-encode', 'finish-local-encode']:
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

elif args.encoder_dump:
    for t in encoder_threads[int(args.encoder)]:
        last = 0
        for e in t.events:
            print (e[0].float_seconds() - last), e[1]
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
        local_encoding = Time()
        sending = Time()
        remote_encoding = Time()
        receiving = Time()
        wakes = 0
        for e in t.events:
            if last is not None:
                if last[1] == 'encoder-sleep':
                    asleep += e[0] - last[0]
                elif last[1] == 'encoder-wake':
                    wakes += 1
                elif last[1] == 'start-local-encode':
                    local_encoding += e[0] - last[0]
                elif last[1] == 'start-remote-send':
                    sending += e[0] - last[0]
                elif last[1] == 'start-remote-encode':
                    remote_encoding += e[0] - last[0]
                elif last[1] == 'start-remote-receive':
                    receiving += e[0] - last[0]
                elif last[1] == 'start-encoder-thread':
                    find_encoder_thread(last[2]['thread']).server = last[2]['server']

            last = e

        print '-- Encoder thread %s (%s)' % (t.server, t.id)
        print '\tAwoken %d times' % wakes

        total = asleep.float_seconds() + local_encoding.float_seconds() + sending.float_seconds() + remote_encoding.float_seconds() + receiving.float_seconds()
        if total == 0:
            continue

        print '\t%s: %2.f%% %fs' % ('Asleep'.ljust(16), asleep.float_seconds() * 100 / total, asleep.float_seconds())

        def print_with_fps(v, name, total, frames):
            if v.float_seconds() > 1:
                print '\t%s: %2.f%% %f %.2ffps' % (name.ljust(16), v.float_seconds() * 100 / total, v.float_seconds(), frames / v.float_seconds())

        print_with_fps(local_encoding, 'Local encoding', total, wakes)
        if sending.float_seconds() > 0:
            print '\t%s: %2.f%%' % ('Sending'.ljust(16), sending.float_seconds() * 100 / total)
        print_with_fps(remote_encoding, 'Remote encoding', total, wakes)
        if receiving.float_seconds() > 0:
            print '\t%s: %2.f%%' % ('Receiving'.ljust(16), receiving.float_seconds() * 100 / total)
        print ''
