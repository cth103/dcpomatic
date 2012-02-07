import os
import subprocess
import shlex
import shutil
import player

class Film:
    def __init__(self, data = None):
        # File or directory containing content
        self.content = None
        # True if content is in DVD format
        self.dvd = False
        # DVD title number
        self.dvd_title = 1
        # Directory containing metadata
        self.data = None
        # Film name
        self.name = None
        # Number of pixels by which to crop the content from each edge
        self.left_crop = 0
        self.top_crop = 0
        self.right_crop = 0
        self.bottom_crop = 0
        # Use deinterlacing filter
        self.deinterlace = False
        # Target ratio
        self.ratio = 1.85
        # Audio stream ID to play
        self.aid = None

        self.width = None
        self.height = None
        self.fps = None
        self.length = None

        if data is not None:
            self.data = data
            f = open(os.path.join(self.data, 'info'), 'r')
            while 1:
                l = f.readline()
                if l == '':
                    break

                d = l.strip()
            
                s = d.find(' ')
                if s != -1:
                    key = d[:s]
                    value = d[s+1:]
                
                    if key == 'name':
                        self.name = value
                    elif key == 'content':
                        self.content = value
                    elif key == 'dvd':
                        self.dvd = int(value) == 1
                    elif key == 'dvd_title':
                        self.dvd_title = int(value)
                    elif key == 'left_crop':
                        self.left_crop = int(value)
                    elif key == 'top_crop':
                        self.top_crop = int(value)
                    elif key == 'right_crop':
                        self.right_crop = int(value)
                    elif key == 'bottom_crop':
                        self.bottom_crop = int(value)
                    elif key == 'deinterlace':
                        self.deinterlace = int(value) == 1
                    elif key == 'ratio':
                        self.ratio = float(value)
                    elif key == 'aid':
                        self.aid = int(value)
                    elif key == 'width':
                        self.width = int(value)
                    elif key == 'height':
                        self.height = int(value)
                    elif key == 'fps':
                        self.fps = float(value)
                    elif key == 'length':
                        self.length = float(value)

        if self.width is None or self.height is None or self.fps is None or self.length is None:
            self.update_content_metadata()

    def write(self):
        try:
            os.mkdir(self.data)
        except OSError:
            pass

        f = open(os.path.join(self.data, 'info'), 'w')
        self.write_datum(f, 'name', self.name)
        self.write_datum(f, 'content', self.content)
        self.write_datum(f, 'dvd', int(self.dvd))
        self.write_datum(f, 'dvd_title', self.dvd_title)
        self.write_datum(f, 'left_crop', self.left_crop)
        self.write_datum(f, 'top_crop', self.top_crop)
        self.write_datum(f, 'right_crop', self.right_crop)
        self.write_datum(f, 'bottom_crop', self.bottom_crop)
        self.write_datum(f, 'deinterlace', int(self.deinterlace))
        self.write_datum(f, 'ratio', self.ratio)
        self.write_datum(f, 'aid', self.aid)
        self.write_datum(f, 'width', self.width)
        self.write_datum(f, 'height', self.height)
        self.write_datum(f, 'fps', self.fps)
        self.write_datum(f, 'length', self.length)

    def write_datum(self, f, key, value):
        if value is not None:
            print >>f,'%s %s' % (key, str(value))

    def thumbs_dir(self):
        t = os.path.join(self.data, 'thumbs')

        try:
            os.mkdir(t)
        except OSError:
            pass

        return t

    def thumb(self, n):
        return os.path.join(self.thumbs_dir(), str('%08d.png' % (n + 1)))

    def thumbs(self):
        return len(os.listdir(self.thumbs_dir()))

    def remove_thumbs(self):
        shutil.rmtree(self.thumbs_dir())

    def make_thumbs(self):
        num_thumbs = 128
        cl = self.player_command_line()
        if self.length is not None:
            sstep = self.length / num_thumbs
        else:
            sstep = 100
        cl.extra = '-vo png -frames %d -sstep %d -nosound' % (num_thumbs, sstep)
        os.chdir(self.thumbs_dir())
        os.system(cl.get(True))

    def set_dvd(self, d):
        self.dvd = d
        self.remove_thumbs()

    def set_dvd_title(self, t):
        self.dvd_title = t
        self.remove_thumbs()

    def set_content(self, c):
        if c == self.content:
            return

        self.content = c
        self.update_content_metadata()

    def player_command_line(self):
        cl = player.CommandLine()
        cl.dvd = self.dvd
        cl.dvd_title = self.dvd_title
        cl.content = self.content
        return cl
    
    def update_content_metadata(self):
        if self.content is None:
            return

        self.width = None
        self.height = None
        self.fps = None
        self.length = None

        cl = self.player_command_line()
        cl.extra = '-identify -vo null -ao null -frames 0'
        text = subprocess.check_output(shlex.split(cl.get(True))).decode('utf-8')
        lines = text.split('\n')
        for l in lines:
            s = l.strip()
            b = s.split('=')
            if len(b) == 2:
                if b[0] == 'ID_VIDEO_WIDTH':
                    self.width = int(b[1])
                elif b[0] == 'ID_VIDEO_HEIGHT':
                    self.height = int(b[1])
                elif b[0] == 'ID_VIDEO_FPS':
                    self.fps = float(b[1])
                elif b[0] == 'ID_LENGTH':
                    self.length = float(b[1])
