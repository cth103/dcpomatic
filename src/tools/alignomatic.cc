/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <gtkmm.h>
#include "lib/util.h"
#include "lib/config.h"
#include "lib/screen.h"
#include "lib/format.h"
#include "gtk/gtk_util.h"
#include "gtk/alignment.h"

using namespace std;
using namespace boost;

static Alignment* alignment = 0;
static Gtk::ComboBoxText* format_combo = 0;
static Format const * format = 0;
static Gtk::ComboBoxText* screen_combo = 0;
static shared_ptr<Screen> screen;
static Gtk::Button* add_screen = 0;
static Gtk::Entry* screen_name = 0;
static Gtk::SpinButton* x_position = 0;
static Gtk::SpinButton* y_position = 0;
static Gtk::SpinButton* width = 0;
static Gtk::Button* calculate_width = 0;
static Gtk::SpinButton* height = 0;
static Gtk::Button* calculate_height = 0;
static Gtk::Button* save = 0;
static bool screen_dirty = false;

enum GeometryPart {
	GEOMETRY_PART_X,
	GEOMETRY_PART_Y,
	GEOMETRY_PART_WIDTH,
	GEOMETRY_PART_HEIGHT
};

void
update_sensitivity ()
{
	bool const dims = format && screen;

	x_position->set_sensitive (dims);
	y_position->set_sensitive (dims);
	width->set_sensitive (dims);
	calculate_width->set_sensitive (dims);
	height->set_sensitive (dims);
	calculate_height->set_sensitive (dims);

	screen_name->set_sensitive (screen);
	save->set_sensitive (screen_dirty);
}

void
update_alignment ()
{
	if (!screen || !format) {
		return;
	}

	delete alignment;
	alignment = new Alignment (screen->position (format), screen->size (format));
	alignment->set_text_line (0, screen->name ());
	alignment->set_text_line (1, format->name ());
}

void
update_entries ()
{
	if (!screen || !format) {
		return;
	}
	
	Position p = screen->position (format);
	x_position->set_value (p.x);
	y_position->set_value (p.y);
	Size s = screen->size (format);
	width->set_value (s.width);
	height->set_value (s.height);

	update_sensitivity ();
}

void
screen_changed ()
{
	if (screen_combo->get_active_row_number() < 0) {
		return;
	}

	vector<shared_ptr<Screen> > screens = Config::instance()->screens ();
	
	if (screens[screen_combo->get_active_row_number()] == screen) {
		return;
	}
	
	screen = screens[screen_combo->get_active_row_number()];

	update_entries ();
	update_alignment ();

	screen_name->set_text (screen->name ());

	screen_dirty = false;
	update_sensitivity ();
}

void
format_changed ()
{
	vector<Format const *> formats = Format::all ();
	
	if (formats[format_combo->get_active_row_number()] == format) {
		return;
	}
	
	format = formats[format_combo->get_active_row_number()];
	
	update_entries ();
	update_alignment ();
	update_sensitivity ();
}

void
geometry_changed (GeometryPart p)
{
	if (p == GEOMETRY_PART_X && screen->position(format).x == x_position->get_value_as_int()) {
		return;
	}

	if (p == GEOMETRY_PART_Y && screen->position(format).y == y_position->get_value_as_int()) {
		return;
	}

	if (p == GEOMETRY_PART_WIDTH && screen->size(format).width == width->get_value_as_int()) {
		return;
	}

	if (p == GEOMETRY_PART_HEIGHT && screen->size(format).height == height->get_value_as_int()) {
		return;
	}
	
	screen->set_geometry (
		format,
		Position (x_position->get_value_as_int(), y_position->get_value_as_int()),
		Size (width->get_value_as_int(), height->get_value_as_int())
		);

	update_alignment ();

	screen_dirty = true;
	update_sensitivity ();
}

void
save_clicked ()
{
	Config::instance()->write ();
	screen_dirty = false;
	update_sensitivity ();
}

void
calculate_width_clicked ()
{
	width->set_value (height->get_value_as_int() * format->ratio_as_float ());
}

void
calculate_height_clicked ()
{
	height->set_value (width->get_value_as_int() / format->ratio_as_float ());
}

void
update_screen_combo ()
{
	screen_combo->clear ();
	
	vector<shared_ptr<Screen> > screens = Config::instance()->screens ();
	for (vector<shared_ptr<Screen> >::iterator i = screens.begin(); i != screens.end(); ++i) {
		screen_combo->append_text ((*i)->name ());
	}
}

void
screen_name_changed ()
{
	screen->set_name (screen_name->get_text ());

	int const r = screen_combo->get_active_row_number ();
	update_screen_combo ();
	screen_combo->set_active (r);

	screen_dirty = true;
	update_sensitivity ();
}

void
add_screen_clicked ()
{
	shared_ptr<Screen> s (new Screen ("New Screen"));
	vector<shared_ptr<Screen> > screens = Config::instance()->screens ();
	screens.push_back (s);
	Config::instance()->set_screens (screens);
	update_screen_combo ();
	screen_combo->set_active (screens.size() - 1);
}

int
main (int argc, char* argv[])
{
	dvdomatic_setup ();
	
	Gtk::Main kit (argc, argv);
	
	Gtk::Dialog dialog ("Align-o-matic");

	screen_combo = Gtk::manage (new Gtk::ComboBoxText);
	update_screen_combo ();
	screen_combo->signal_changed().connect (sigc::ptr_fun (&screen_changed));

	add_screen = Gtk::manage (new Gtk::Button ("Add"));
	add_screen->signal_clicked().connect (sigc::ptr_fun (&add_screen_clicked));
	
	screen_name = Gtk::manage (new Gtk::Entry ());
	screen_name->signal_changed().connect (sigc::ptr_fun (&screen_name_changed));
	
	format_combo = Gtk::manage (new Gtk::ComboBoxText);
	vector<Format const *> formats = Format::all ();
	for (vector<Format const *>::iterator i = formats.begin(); i != formats.end(); ++i) {
		format_combo->append_text ((*i)->name ());
	}

	format_combo->signal_changed().connect (sigc::ptr_fun (&format_changed));

	save = Gtk::manage (new Gtk::Button ("Save"));
	save->signal_clicked().connect (sigc::ptr_fun (&save_clicked));

	x_position = Gtk::manage (new Gtk::SpinButton ());
	x_position->signal_value_changed().connect (sigc::bind (ptr_fun (&geometry_changed), GEOMETRY_PART_X));
	x_position->set_range (0, 2048);
	x_position->set_increments (1, 16);
	y_position = Gtk::manage (new Gtk::SpinButton ());
	y_position->signal_value_changed().connect (sigc::bind (sigc::ptr_fun (&geometry_changed), GEOMETRY_PART_Y));
	y_position->set_range (0, 1080);
	y_position->set_increments (1, 16);
	width = Gtk::manage (new Gtk::SpinButton ());
	width->signal_value_changed().connect (sigc::bind (sigc::ptr_fun (&geometry_changed), GEOMETRY_PART_WIDTH));
	width->set_range (0, 2048);
	width->set_increments (1, 16);
	height = Gtk::manage (new Gtk::SpinButton ());
	height->signal_value_changed().connect (sigc::bind (sigc::ptr_fun (&geometry_changed), GEOMETRY_PART_HEIGHT));
	height->set_range (0, 1080);
	height->set_increments (1, 16);

	calculate_width = Gtk::manage (new Gtk::Button ("Calculate"));
	calculate_width->signal_clicked().connect (sigc::ptr_fun (&calculate_width_clicked));
	calculate_height = Gtk::manage (new Gtk::Button ("Calculate"));
	calculate_height->signal_clicked().connect (sigc::ptr_fun (&calculate_height_clicked));

	Gtk::Table table;
	table.set_row_spacings (12);
	table.set_col_spacings (12);
	table.set_border_width (12);
	
	int n = 0;
	table.attach (left_aligned_label ("Screen"), 0, 1, n, n + 1);
	table.attach (*screen_combo, 1, 2, n, n + 1);
	table.attach (*add_screen, 2, 3, n, n + 1);
	++n;
	table.attach (left_aligned_label ("Screen Name"), 0, 1, n, n + 1);
	table.attach (*screen_name, 1, 2, n, n + 1);
	++n;
	table.attach (left_aligned_label ("Format"), 0, 1, n, n + 1);
	table.attach (*format_combo, 1, 2, n, n + 1);
	++n;
	table.attach (left_aligned_label ("x"), 0, 1, n, n + 1);
	table.attach (*x_position, 1, 2, n, n + 1);
	++n;
	table.attach (left_aligned_label ("y"), 0, 1, n, n + 1);
	table.attach (*y_position, 1, 2, n, n + 1);
	++n;
	table.attach (left_aligned_label ("Width"), 0, 1, n, n + 1);
	table.attach (*width, 1, 2, n, n + 1);
	table.attach (*calculate_width, 2, 3, n, n + 1);
	++n;
	table.attach (left_aligned_label ("Height"), 0, 1, n, n + 1);
	table.attach (*height, 1, 2, n, n + 1);
	table.attach (*calculate_height, 2, 3, n, n + 1);
	++n;

	dialog.get_vbox()->pack_start (table, false, false);
	dialog.add_action_widget (*save, 0);
	update_sensitivity ();
	dialog.show_all ();

	Gtk::Main::run (dialog);

	return 0;
}
