import os
import pygtk
pygtk.require('2.0')
import gtk
import ratio
import util
import thumbs

class FilmView(gtk.HBox):
    def __init__(self, parent):
        gtk.HBox.__init__(self)

        self.parent_window = parent

        self.inhibit_save = True
        
        self.table = gtk.Table()
        self.pack_start(self.table, True, True)

        self.table.set_row_spacings(4)
        self.table.set_col_spacings(12)
        self.film_name = gtk.Entry()
        self.ratio = gtk.combo_box_new_text()
        for r in ratio.ratios:
            self.ratio.append_text(r.name())
        self.content_file_radio = gtk.RadioButton()
        self.content_file_radio.set_label("File")
        self.content_file_chooser = gtk.FileChooserDialog("Content", self.parent_window, gtk.FILE_CHOOSER_ACTION_OPEN, (("Select", gtk.RESPONSE_OK)))
        self.content_file_button = gtk.FileChooserButton(self.content_file_chooser)
        self.content_folder_radio = gtk.RadioButton()
        self.content_folder_radio.set_label("Folder")
        self.content_folder_radio.set_group(self.content_file_radio)
        self.content_folder_chooser = gtk.FileChooserDialog("Content", self.parent_window, gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER, (("Select", gtk.RESPONSE_OK)))
        self.content_folder_button = gtk.FileChooserButton(self.content_folder_chooser)
        self.dvd = gtk.CheckButton("DVD")
        self.dvd_title = gtk.SpinButton()
        self.dvd_title.set_range(0, 32)
        self.dvd_title.set_increments(1, 4)
        self.deinterlace = gtk.CheckButton("Deinterlace")
        self.left_crop = self.crop_spinbutton()
        self.right_crop = self.crop_spinbutton()
        self.top_crop = self.crop_spinbutton()
        self.bottom_crop = self.crop_spinbutton()
        
        # Information about the content (immutable)
        self.source_size = self.label()
        self.fps = self.label()
        self.length = self.label()

        # Buttons
        self.thumbs_button = gtk.Button("Show Thumbnails")

        self.film_name.connect("changed", self.changed, self)
        self.ratio.connect("changed", self.changed, self)
        self.deinterlace.connect("toggled", self.changed, self)
        self.thumbs_button.connect("clicked", self.thumbs_clicked, self)
        self.content_file_radio.connect("toggled", self.content_radio_toggled, self)
        self.content_folder_radio.connect("toggled", self.content_radio_toggled, self)
        self.content_file_button.connect("file-set", self.content_file_chooser_file_set, self)
        self.content_folder_button.connect("file-set", self.content_folder_chooser_file_set, self)
        self.dvd.connect("toggled", self.changed, self)
        self.dvd_title.connect("value-changed", self.changed, self)

        n = 0
        self.table.attach(self.label("Film"), 0, 1, n, n + 1)
        self.table.attach(self.film_name, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Ratio"), 0, 1, n, n + 1)
        self.table.attach(self.ratio, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Content"), 0, 1, n, n + 1)
        b = gtk.HBox()
        b.set_spacing(4)
        b.pack_start(self.content_file_radio, False, False)
        b.pack_start(self.content_file_button, True, True)
        b.pack_start(self.content_folder_radio, False, False)
        b.pack_start(self.content_folder_button, True, True)
        self.table.attach(b, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.dvd, 0, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("DVD title"), 0, 1, n, n + 1)
        self.table.attach(self.dvd_title, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.deinterlace, 0, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Left Crop"), 0, 1, n, n + 1)
        self.table.attach(self.left_crop, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Right Crop"), 0, 1, n, n + 1)
        self.table.attach(self.right_crop, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Top Crop"), 0, 1, n, n + 1)
        self.table.attach(self.top_crop, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Bottom Crop"), 0, 1, n, n + 1)
        self.table.attach(self.bottom_crop, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Source size"), 0, 1, n, n + 1)
        self.table.attach(self.source_size, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Frames per second"), 0, 1, n, n + 1)
        self.table.attach(self.fps, 1, 2, n, n + 1)
        n += 1
        self.table.attach(self.label("Length"), 0, 1, n, n + 1)
        self.table.attach(self.length, 1, 2, n, n + 1)

        self.right_vbox = gtk.VBox()
        self.pack_start(self.right_vbox, False, False)

        self.right_vbox.pack_start(self.thumbs_button, False, False)

        self.inhibit_save = False

    def set(self, film):
        self.inhibit_save = True

        self.film = film
        self.film_name.set_text(film.name)
        self.ratio.set_active(ratio.ratio_to_index(film.ratio))
        if film.content is not None:
            if os.path.isfile(film.content):
                self.set_content_is_file(True)
                self.content_file_button.set_filename(film.content)
            else:
                self.set_content_is_file(False)
                self.content_folder_button.set_filename(film.content)
        self.dvd.set_active(film.dvd)
        self.dvd_title.set_value(film.dvd_title)
        self.dvd_title.set_sensitive(film.dvd)
        self.deinterlace.set_active(film.deinterlace)
        self.left_crop.set_value(film.left_crop)
        self.right_crop.set_value(film.right_crop)
        self.top_crop.set_value(film.top_crop)
        self.bottom_crop.set_value(film.bottom_crop)

        # Content information
        if film.width is not None and film.height is not None:
            self.source_size.set_text("%dx%d" % (film.width, film.height))
        else:
            self.source_size.set_text("Unknown")
        if film.fps is not None:
            self.fps.set_text(str(film.fps))
        if film.length is not None:
            self.length.set_text("%d:%02d:%02d" % util.s_to_hms(film.length))

        self.inhibit_save = False

    def set_content_is_file(self, f):
        self.content_file_button.set_sensitive(f)
        self.content_folder_button.set_sensitive(not f)
        self.content_file_radio.set_active(f)
        self.content_folder_radio.set_active(not f)
    
    def label(self, text = ""):
        l = gtk.Label(text)
        l.set_alignment(0, 0.5)
        return l

    def changed(self, a, b):
        self.dvd_title.set_sensitive(self.dvd.get_active())
        self.save_film()

    def crop_spinbutton(self):
        s = gtk.SpinButton()
        s.set_range(0, 1024)
        s.set_increments(1, 16)
        s.connect("value-changed", self.changed, self)
        return s

    def save_film(self):
        if self.inhibit_save:
            return

        self.film.name = self.film_name.get_text()
        self.film.ratio = ratio.index_to_ratio(self.ratio.get_active()).ratio

        if self.content_file_radio.get_active():
            self.film.set_content(self.content_file_button.get_filename())
        else:
            self.film.set_content(self.content_folder_button.get_filename())
        self.film.set_dvd(self.dvd.get_active())
        self.film.set_dvd_title(self.dvd_title.get_value_as_int())
        self.film.deinterlace = self.deinterlace.get_active()
        self.film.left_crop = self.left_crop.get_value_as_int()
        self.film.right_crop = self.right_crop.get_value_as_int()
        self.film.top_crop = self.top_crop.get_value_as_int()
        self.film.bottom_crop = self.bottom_crop.get_value_as_int()
        self.film.write()

    def thumbs_clicked(self, a, b):
        if self.film.thumbs() == 0:
            self.film.make_thumbs()

        t = thumbs.Thumbs(self.film)
        t.run()
        t.hide()
        self.left_crop.set_value(t.left_crop())
        self.right_crop.set_value(t.right_crop())
        self.top_crop.set_value(t.top_crop())
        self.bottom_crop.set_value(t.bottom_crop())

    def content_file_chooser_file_set(self, a, b):
        self.changed(a, b)

    def content_folder_chooser_file_set(self, a, b):
        self.changed(a, b)

    def content_radio_toggled(self, a, b):
        self.set_content_is_file(self.content_file_radio.get_active())
        self.changed(a, b)

