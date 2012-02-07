#!/usr/bin/python

class Screen:
    def __init__(self):
        self.name = None
        self.formats = []

class Format:
    def __init__(self):
        self.ratio = None
        self.x = None
        self.y = None
        self.width = None
        self.height = None
        self.external = False

class Screens:
    def __init__(self, file):

        self.screens = []

        f = open(file, 'r')
        current_screen = None
        current_format = None
        while 1:
            l = f.readline()
            if l == '':
                break
            if len(l) > 0 and l[0] == '#':
                continue

            s = l.strip()

            if len(s) == 0:
                continue

            b = s.split()

            if len(b) != 2:
                print "WARNING: ignored line `%s' in screens file" % (s)
                continue

            if b[0] == 'screen':
                if current_format is not None:
                    current_screen.formats.append(current_format)
                    current_format = None

                if current_screen is not None:
                    self.screens.append(current_screen)
                    current_screen = None
                
                current_screen = Screen()
                current_screen.name = b[1]
            elif b[0] == 'ratio':
                if current_format is not None:
                    current_screen.formats.append(current_format)
                    current_format = None
                    
                current_format = Format()
                current_format.ratio = float(b[1])
            elif b[0] == 'x':
                current_format.x = int(b[1])
            elif b[0] == 'y':
                current_format.y = int(b[1])
            elif b[0] == 'width':
                current_format.width = int(b[1])
            elif b[0] == 'height':
                current_format.height = int(b[1])
            elif b[0] == 'external':
                current_format.external = int(b[1]) == 1

        if current_format is not None:
            current_screen.formats.append(current_format)

        if current_screen is not None:
            self.screens.append(current_screen)

    def get_format(self, screen, ratio):
        for s in self.screens:
            if s.name == screen:
                for f in s.formats:
                    if f.ratio == ratio:
                        return f

        return None
