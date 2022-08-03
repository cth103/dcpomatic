#!/usr/bin/python

import subprocess
import shlex
import sys

AUDIO_STREAM = "1"
types = ['video']
VIDEO_RATE = 29.97

last_video = None
last_video_pts = None

last_audio_pts = {}
last_channels = {}

video_frame_rates = []

video_frame_count = 0

def check_pts_dts(frame):
    if 'pkt_dts_time' in frame:
        diff = frame['pkt_pts_time'] - frame['pkt_dts_time']
        if abs(diff) > 1e-8:
            print("\tPTS != DTS: %f" % diff)

def handle(frame):
    global last_video
    global last_video_pts
    global last_audio_pts
    global video_frame_count
    global video_frame_rates
    if frame['media_type'] == 'video' and 'video' in types:
        if last_video_pts is not None and frame['pkt_pts_time'] <= last_video_pts:
            print('Out of order video frame %f (%d) is same as or behind %f (%d)' % (frame['pkt_pts_time'], frame['pkt_pts'], last_video_pts, last_video))
        elif last_video_pts is not None:
            pkt_dts_time = frame['pkt_dts_time'] if 'pkt_dts_time' in frame else 0.0
            rate = 1 / (frame['pkt_pts_time'] - last_video_pts)
            print(f'OK V    frame %f %f %f %f %d indices %d/%d' % (frame['pkt_pts_time'], pkt_dts_time, frame['pkt_pts_time'] - last_video_pts, rate, frame['pkt_size'], video_frame_count, frame['pkt_pts_time'] * VIDEO_RATE))
            check_pts_dts(frame)
            video_frame_rates.append(rate)
        else:
            print('OK V    frame %f counted %d' % (frame['pkt_pts_time'], video_frame_count))
        last_video = frame['pkt_pts']
        last_video_pts = frame['pkt_pts_time']
        video_frame_count += 1
    elif frame['media_type'] == 'audio' and 'audio' in types:
        stream_index = frame['stream_index']
        if stream_index in last_audio_pts and (stream_index == AUDIO_STREAM or AUDIO_STREAM is None):
            print('OK A[%s] frame %4.8f %4.8f %4.8f %4.8f %d' % (stream_index, frame['pkt_pts_time'], frame['pkt_dts_time'], frame['pkt_pts_time'] - last_audio_pts[stream_index], 1 / (frame['pkt_pts_time'] - last_audio_pts[stream_index]), frame['pkt_size']))
            check_pts_dts(frame)
        if stream_index in last_channels and frame['channels'] != last_channels[stream_index]:
            print("\t*** unusual channel count")
        last_audio_pts[stream_index] = frame['pkt_pts_time']
        last_channels[stream_index] = frame['channels']


p = subprocess.Popen(shlex.split('ffprobe -show_frames "%s"' % sys.argv[1]), stdin=None, stdout=subprocess.PIPE)
frame = dict()
while True:
    l = p.stdout.readline()
    if l == b'':
        break

    l = l.strip().decode('UTF-8')

    if l == '[/FRAME]':
        handle(frame)
        frame = dict()
    elif l != '[FRAME]' and l != '[SIDE_DATA]' and l != '[/SIDE_DATA]':
        s = l.split('=')
        if s[0] in ['pkt_pts_time', 'pkt_dts_time', 'pkt_pts', 'pkt_dts']:
            if s[1] != 'N/A':
                frame[s[0]] = float(s[1])
        elif s[0] in ['channels', 'pkt_size', 'nb_samples']:
            frame[s[0]] = int(s[1])
        elif len(s) > 1:
            frame[s[0]] = s[1]


print(f'Average frame rate: {sum(video_frame_rates) / float(len(video_frame_rates))}')
