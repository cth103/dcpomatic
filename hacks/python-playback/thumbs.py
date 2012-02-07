# GUI to display thumbnails and allow cropping
# to be set up

import os
import sys
import pygtk
pygtk.require('2.0')
import gtk
import film
import player

class Thumbs(gtk.Dialog):
    def __init__(self, film):
        gtk.Dialog.__init__(self)
        self.film = film
        self.controls = gtk.Table()
        self.controls.set_col_spacings(4)
        self.thumb_adj = gtk.Adjustment(0, 0, self.film.thumbs() - 1, 1, 10)
        self.add_control("Thumbnail", self.thumb_adj, 0)
        self.left_crop_adj = gtk.Adjustment(self.film.left_crop, 0, 1024, 1, 128)
        self.add_control("Left crop", self.left_crop_adj, 1)
        self.right_crop_adj = gtk.Adjustment(self.film.right_crop, 0, 1024, 1, 128)
        self.add_control("Right crop", self.right_crop_adj, 2)
        self.top_crop_adj = gtk.Adjustment(self.film.top_crop, 0, 1024, 1, 128)
        self.add_control("Top crop", self.top_crop_adj, 3)
        self.bottom_crop_adj = gtk.Adjustment(self.film.bottom_crop, 0, 1024, 1, 128)
        self.add_control("Bottom crop", self.bottom_crop_adj, 4)
        self.display_image = gtk.Image()
        self.update_display()
        window_box = gtk.HBox()
        window_box.set_spacing(12)

        controls_vbox = gtk.VBox()
        controls_vbox.set_spacing(4)
        controls_vbox.pack_start(self.controls, False, False)
        
        window_box.pack_start(controls_vbox, True, True)
        window_box.pack_start(self.display_image)
        
        self.set_title("%s Thumbnails" % film.name)
        self.get_content_area().add(window_box)
        self.add_button("Close", gtk.RESPONSE_ACCEPT)
        self.show_all()

        for a in [self.thumb_adj, self.left_crop_adj, self.right_crop_adj, self.top_crop_adj, self.bottom_crop_adj]:
            a.connect('value-changed', self.update_display, self)

    def add_control(self, name, adj, n):
        l = gtk.Label(name)
        l.set_alignment(1, 0.5)
        self.controls.attach(l, 0, 1, n, n + 1)
        s = gtk.SpinButton(adj)
        self.controls.attach(s, 1, 2, n, n + 1)

    def update_display(self, a = None, b = None):
        thumb_pixbuf = gtk.gdk.pixbuf_new_from_file(self.film.thumb(self.thumb_adj.get_value()))
        self.width = thumb_pixbuf.get_width()
        self.height = thumb_pixbuf.get_height()
        left = self.left_crop()
        right = self.right_crop()
        top = self.top_crop()
        bottom = self.bottom_crop()
        pixbuf = thumb_pixbuf.subpixbuf(left, top, self.width - left - right, self.height - top - bottom)
        self.display_image.set_from_pixbuf(pixbuf)

    def top_crop(self):
        return int(self.top_crop_adj.get_value())

    def bottom_crop(self):
        return int(self.bottom_crop_adj.get_value())

    def left_crop(self):
        return int(self.left_crop_adj.get_value())
    
    def right_crop(self):
        return int(self.right_crop_adj.get_value())
