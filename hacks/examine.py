#!/usr/bin/python

import subprocess
import shlex
import sys

AUDIO_STREAM = "1"
types = ['audio']

last_video = None
last_video_pts = None

last_audio_pts = {}
last_channels = {}

def check_pts_dts(frame):
    diff = frame['pkt_pts_time'] - frame['pkt_dts_time']
    if abs(diff) > 1e-8:
        print("\tPTS != DTS: %f" % diff)

def handle(frame):
    global last_video
    global last_video_pts
    global last_audio_pts
    if frame['media_type'] == 'video' and 'video' in types:
        if last_video_pts is not None and frame['pkt_pts_time'] <= last_video_pts:
            print 'Out of order video frame %f (%d) is same as or behind %f (%d)' % (frame['pkt_pts_time'], frame['pkt_pts'], last_video_pts, last_video)
        elif last_video_pts is not None:
            print 'OK V    frame %f %f %f %f %d' % (frame['pkt_pts_time'], frame['pkt_dts_time'], frame['pkt_pts_time'] - last_video_pts, 1 / (frame['pkt_pts_time'] - last_video_pts), frame['pkt_size'])
            check_pts_dts(frame)
	else:
            print 'OK V    frame %f' % (frame['pkt_pts_time'])
        last_video = frame['pkt_pts']
        last_video_pts = frame['pkt_pts_time']
    elif frame['media_type'] == 'audio' and 'audio' in types:
	stream_index = frame['stream_index']
	if stream_index in last_audio_pts and (stream_index == AUDIO_STREAM or AUDIO_STREAM is None):
            print 'OK A[%s] frame %4.8f %4.8f %4.8f %4.8f %d' % (stream_index, frame['pkt_pts_time'], frame['pkt_dts_time'], frame['pkt_pts_time'] - last_audio_pts[stream_index], 1 / (frame['pkt_pts_time'] - last_audio_pts[stream_index]), frame['pkt_size'])
            check_pts_dts(frame)
        if stream_index in last_channels and frame['channels'] != last_channels[stream_index]:
            print "\t*** unusual channel count"
        last_audio_pts[stream_index] = frame['pkt_pts_time']
	last_channels[stream_index] = frame['channels']


p = subprocess.Popen(shlex.split('ffprobe -show_frames "%s"' % sys.argv[1]), stdin=None, stdout=subprocess.PIPE)
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
	if s[0] in ['pkt_pts_time', 'pkt_dts_time', 'pkt_pts', 'pkt_dts']:
            frame[s[0]] = float(s[1])
	elif s[0] in ['channels', 'pkt_size', 'nb_samples']:
	    frame[s[0]] = int(s[1])
        elif len(s) > 1:
            frame[s[0]] = s[1]
