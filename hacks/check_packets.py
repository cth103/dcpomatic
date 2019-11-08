#!/usr/bin/python

import subprocess
import shlex
import sys

last_video = None
last_video_pts = None

def handle(frame):
    global last_video
    global last_video_pts
    if frame['media_type'] == 'video':
        if last_video_pts is not None and frame['pkt_pts_time'] <= last_video_pts:
            print 'Out of order video frame %f (%d) is same as or behind %f (%d)' % (frame['pkt_pts_time'], frame['pkt_pts'], last_video_pts, last_video)
        else:
            print 'OK frame %f' % frame['pkt_pts_time']
        last_video = frame['pkt_pts']
        last_video_pts = frame['pkt_pts_time']

p = subprocess.Popen(shlex.split('ffprobe -show_frames %s' % sys.argv[1]), stdin=None, stdout=subprocess.PIPE)
frame = dict()
while True:
    l = p.stdout.readline()
    if l == '':
        break

    l = l.strip()

    if l == '[/FRAME]':
        handle(frame)
        frame = dict()
    elif l != '[FRAME]' and l != '[SIDE_DATA]' and l != '[/SIDE_DATA]':
        s = l.split('=')
        if s[0] == 'pkt_pts_time':
            frame[s[0]] = float(s[1])
        elif s[0] == 'pkt_pts':
            frame[s[0]] = float(s[1]) 
        elif len(s) > 1:
            frame[s[0]] = s[1]
