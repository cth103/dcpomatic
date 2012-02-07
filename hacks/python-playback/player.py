import os
import threading
import subprocess
import shlex
import select
import film
import config
import mplayer

class CommandLine:
    def __init__(self):
        self.position_x = 0
        self.position_y = 0
        self.output_width = None
        self.output_height = None
        self.mov = False
        self.delay = None
        self.dvd = False
        self.dvd_title = 1
        self.content = None
        self.extra = ''
        self.crop_x = None
        self.crop_y = None
        self.crop_w = None
        self.crop_h = None
        self.deinterlace = False
        self.aid = None
        
    def get(self, with_binary):
        # hqdn3d?
        # nr, unsharp?
        # -vo x11 appears to be necessary to prevent unwanted hardware scaling
        # -noaspect stops mplayer rescaling to the movie's specified aspect ratio
        args = '-vo x11 -noaspect -ao pulse -noborder -noautosub -nosub -sws 10 '
        args += '-geometry %d:%d ' % (self.position_x, self.position_y)

        # Video filters (passed to -vf)

        filters = []

        if self.crop_x is not None or self.crop_y is not None or self.crop_w is not None or self.crop_h is not None:
            crop = 'crop='
            if self.crop_w is not None and self.crop_h is not None:
                crop += '%d:%d' % (self.crop_w, self.crop_h)
                if self.crop_x is not None and self.crop_x is not None:
                    crop += ':%d:%d' % (self.crop_x, self.crop_y)
                filters.append(crop)

        if self.output_width is not None or self.output_height is not None:
            filters.append('scale=%d:%d' % (self.output_width, self.output_height))

        # Post processing
        pp = []
        if self.deinterlace:
            pp.append('lb')

        # Deringing filter
        pp.append('dr')

        if len(pp) > 0:
            pp_string = 'pp='
            for i in range(0, len(pp)):
                pp_string += pp[i]
                if i < len(pp) - 1:
                    pp += ','

            filters.append(pp_string)

        if len(filters) > 0:
            args += '-vf '
            for i in range(0, len(filters)):
                args += filters[i]
                if i < len(filters) - 1:
                    args += ','
            args += ' '

        if self.mov:
            args += '-demuxer mov '
        if self.delay is not None:
            args += '-delay %f ' % float(args.delay)
        if self.aid is not None:
            args += '-aid %s ' % self.aid

        args += self.extra
        
        if self.dvd:
            data_specifier = 'dvd://%d -dvd-device \"%s\"' % (self.dvd_title, self.content)
        else:
            data_specifier = '\"%s\"' % self.content

        if with_binary:
            return 'mplayer %s %s' % (args, data_specifier)
        
        return '%s %s' % (args, data_specifier)
  
def get_player(film, format):
    cl = CommandLine()
    cl.dvd = film.dvd
    cl.dvd_title = film.dvd_title
    cl.content = film.content
    cl.crop_w = film.width - film.left_crop - film.right_crop
    cl.crop_h = film.height - film.top_crop - film.bottom_crop
    cl.position_x = format.x
    if format.external:
        cl.position_x = format.x + config.LEFT_SCREEN_WIDTH
        cl.position_y = format.y
    cl.output_width = format.width
    cl.output_height = format.height
    cl.deinterlace = film.deinterlace
    cl.aid = film.aid
    return mplayer.Player(cl.get(False))

