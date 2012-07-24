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

#include <iostream>
#include <boost/filesystem.hpp>
#include "gtk/film_viewer.h"
#include "gtk/film_editor.h"
#ifndef DVDOMATIC_DISABLE_PLAYER
#include "gtk/film_player.h"
#endif
#include "gtk/job_manager_view.h"
#include "gtk/config_dialog.h"
#include "gtk/gpl.h"
#include "gtk/job_wrapper.h"
#include "gtk/dvd_title_dialog.h"
#include "gtk/gtk_util.h"
#include "lib/film.h"
#include "lib/format.h"
#include "lib/config.h"
#include "lib/filter.h"
#include "lib/util.h"
#include "lib/scaler.h"
#include "lib/exceptions.h"

using namespace std;
using namespace boost;

static Gtk::Window* window = 0;
static FilmViewer* film_viewer = 0;
static FilmEditor* film_editor = 0;
#ifndef DVDOMATIC_DISABLE_PLAYER
static FilmPlayer* film_player = 0;
#endif
static Film* film = 0;

static void set_menu_sensitivity ();

class FilmChangedDialog : public Gtk::MessageDialog
{
public:
	FilmChangedDialog ()
		: Gtk::MessageDialog ("", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE)
	{
		stringstream s;
		s << "Save changes to film \"" << film->name() << "\" before closing?";
		set_message (s.str ());
		add_button ("Close _without saving", Gtk::RESPONSE_NO);
		add_button ("_Cancel", Gtk::RESPONSE_CANCEL);
		add_button ("_Save", Gtk::RESPONSE_YES);
	}
};

bool
maybe_save_then_delete_film ()
{
	if (!film) {
		return false;
	}
			
	if (film->dirty ()) {
		FilmChangedDialog d;
		switch (d.run ()) {
		case Gtk::RESPONSE_CANCEL:
			return true;
		case Gtk::RESPONSE_YES:
			film->write_metadata ();
			break;
		case Gtk::RESPONSE_NO:
			return false;
		}
	}
	
	delete film;
	film = 0;
	return false;
}

void
file_new ()
{
	Gtk::FileChooserDialog c (*window, "New Film", Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER);
#ifdef DVDOMATIC_WINDOWS	
	c.set_current_folder (g_get_user_data_dir ());
#endif	
	c.add_button ("_Cancel", Gtk::RESPONSE_CANCEL);
	c.add_button ("C_reate", Gtk::RESPONSE_ACCEPT);

	int const r = c.run ();
	if (r == Gtk::RESPONSE_ACCEPT) {
		if (maybe_save_then_delete_film ()) {
			return;
		}
		film = new Film (c.get_filename ());
#if BOOST_FILESYSTEM_VERSION == 3		
		film->set_name (filesystem::path (c.get_filename().c_str()).filename().generic_string());
#else		
		film->set_name (filesystem::path (c.get_filename().c_str()).filename());
#endif		
		film_viewer->set_film (film);
		film_editor->set_film (film);
		set_menu_sensitivity ();
	}
}

void
file_open ()
{
	Gtk::FileChooserDialog c (*window, "Open Film", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
#ifdef DVDOMATIC_WINDOWS	
	c.set_current_folder (g_get_user_data_dir ());
#endif	
	c.add_button ("_Cancel", Gtk::RESPONSE_CANCEL);
	c.add_button ("_Open", Gtk::RESPONSE_ACCEPT);

	int const r = c.run ();
	if (r == Gtk::RESPONSE_ACCEPT) {
		if (maybe_save_then_delete_film ()) {
			return;
		}
		film = new Film (c.get_filename ());
		film_viewer->set_film (film);
		film_editor->set_film (film);
		set_menu_sensitivity ();
	}
}

void
file_save ()
{
	film->write_metadata ();
}

void
file_quit ()
{
	if (maybe_save_then_delete_film ()) {
		return;
	}
	
	Gtk::Main::quit ();
}

void
edit_preferences ()
{
	ConfigDialog d;
	d.run ();
	Config::instance()->write ();
}

void
jobs_make_dcp ()
{
	JobWrapper::make_dcp (film, true);
}

void
jobs_make_dcp_from_existing_transcode ()
{
	JobWrapper::make_dcp (film, false);
}

void
jobs_copy_from_dvd ()
{
	try {
		DVDTitleDialog d;
		if (d.run () != Gtk::RESPONSE_OK) {
			return;
		}
		film->copy_from_dvd ();
	} catch (DVDError& e) {
		error_dialog (e.what ());
	}
}

void
jobs_send_dcp_to_tms ()
{
	film->send_dcp_to_tms ();
}

void
jobs_examine_content ()
{
	film->examine_content ();
}

void
help_about ()
{
	Gtk::AboutDialog d;
	d.set_name ("DVD-o-matic");
	d.set_version (DVDOMATIC_VERSION);

	stringstream s;
	s << "DCP generation from arbitrary formats\n\n"
	  << "Using " << dependency_version_summary() << "\n";
	d.set_comments (s.str ());

	vector<string> authors;
	authors.push_back ("Carl Hetherington");
	authors.push_back ("Terrence Meiczinger");
	authors.push_back ("Paul Davis");
	d.set_authors (authors);

	d.set_website ("http://carlh.net/software/dvdomatic");
	d.set_license (gpl);
	
	d.run ();
}

enum Sensitivity {
	ALWAYS,
	NEEDS_FILM
};

map<Gtk::MenuItem *, Sensitivity> menu_items;
	
void
add_item (Gtk::Menu_Helpers::MenuList& items, string text, sigc::slot0<void> handler, Sensitivity sens)
{
	items.push_back (Gtk::Menu_Helpers::MenuElem (text, handler));
	menu_items.insert (make_pair (&items.back(), sens));
}

void
set_menu_sensitivity ()
{
	for (map<Gtk::MenuItem *, Sensitivity>::iterator i = menu_items.begin(); i != menu_items.end(); ++i) {
		if (i->second == NEEDS_FILM) {
			i->first->set_sensitive (film != 0);
		} else {
			i->first->set_sensitive (true);
		}
	}
}

void
setup_menu (Gtk::MenuBar& m)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* file = manage (new Gtk::Menu);
	MenuList& file_items (file->items ());
	add_item (file_items, "New...", sigc::ptr_fun (file_new), ALWAYS);
	add_item (file_items, "_Open...", sigc::ptr_fun (file_open), ALWAYS);
	file_items.push_back (SeparatorElem ());
	add_item (file_items, "_Save", sigc::ptr_fun (file_save), NEEDS_FILM);
	file_items.push_back (SeparatorElem ());
	add_item (file_items, "_Quit", sigc::ptr_fun (file_quit), ALWAYS);

	Gtk::Menu* edit = manage (new Gtk::Menu);
	MenuList& edit_items (edit->items ());
	add_item (edit_items, "_Preferences...", sigc::ptr_fun (edit_preferences), ALWAYS);

	Gtk::Menu* jobs = manage (new Gtk::Menu);
	MenuList& jobs_items (jobs->items ());
	add_item (jobs_items, "_Make DCP", sigc::ptr_fun (jobs_make_dcp), NEEDS_FILM);
	add_item (jobs_items, "_Send DCP to TMS", sigc::ptr_fun (jobs_send_dcp_to_tms), NEEDS_FILM);
	add_item (jobs_items, "Copy from _DVD...", sigc::ptr_fun (jobs_copy_from_dvd), NEEDS_FILM);
	jobs_items.push_back (SeparatorElem ());
	add_item (jobs_items, "_Examine content", sigc::ptr_fun (jobs_examine_content), NEEDS_FILM);
	add_item (jobs_items, "Make DCP from _existing transcode", sigc::ptr_fun (jobs_make_dcp_from_existing_transcode), NEEDS_FILM);

	Gtk::Menu* help = manage (new Gtk::Menu);
	MenuList& help_items (help->items ());
	add_item (help_items, "About", sigc::ptr_fun (help_about), ALWAYS);
	
	MenuList& items (m.items ());
	items.push_back (MenuElem ("_File", *file));
	items.push_back (MenuElem ("_Edit", *edit));
	items.push_back (MenuElem ("_Jobs", *jobs));
	items.push_back (MenuElem ("_Help", *help));
}

bool
window_closed (GdkEventAny *)
{
	if (maybe_save_then_delete_film ()) {
		return true;
	}

	return false;
}

void
file_changed (string f)
{
	stringstream s;
	s << "DVD-o-matic";
	if (!f.empty ()) {
		s << " — " << f;
	}
	
	window->set_title (s.str ());
}

int
main (int argc, char* argv[])
{
	dvdomatic_setup ();
	
	Gtk::Main kit (argc, argv);

	if (argc == 2 && boost::filesystem::is_directory (argv[1])) {
		film = new Film (argv[1]);
	}

	window = new Gtk::Window ();
	window->signal_delete_event().connect (sigc::ptr_fun (window_closed));
	
	film_viewer = new FilmViewer (film);
	film_editor = new FilmEditor (film);
#ifndef DVDOMATIC_DISABLE_PLAYER
	film_player = new FilmPlayer (film);
#endif	
	JobManagerView jobs_view;

	window->set_title ("DVD-o-matic");

	Gtk::VBox vbox;

	Gtk::MenuBar menu_bar;
	vbox.pack_start (menu_bar, false, false);
	setup_menu (menu_bar);
	set_menu_sensitivity ();

	Gtk::HBox hbox;
	hbox.set_spacing (12);

	Gtk::VBox left_vbox;
	left_vbox.set_spacing (12);
	left_vbox.pack_start (film_editor->widget (), false, false);
#ifndef DVDOMATIC_DISABLE_PLAYER	
	left_vbox.pack_start (film_player->widget (), false, false);
#endif	
	hbox.pack_start (left_vbox, false, false);

	Gtk::VBox right_vbox;
	right_vbox.pack_start (film_viewer->widget (), true, true);
	right_vbox.pack_start (jobs_view.widget(), false, false);
	hbox.pack_start (right_vbox, true, true);

	vbox.pack_start (hbox, true, true);

	window->add (vbox);
	window->show_all ();

	/* XXX: calling these here is a bit of a hack */
	film_editor->setup_visibility ();
#ifndef DVDOMATIC_DISABLE_PLAYER	
	film_player->setup_visibility ();
#endif	
	film_viewer->setup_visibility ();

	film_editor->FileChanged.connect (ptr_fun (file_changed));
	if (film) {
		file_changed (film->directory ());
	} else {
		file_changed ("");
	}

	/* XXX this should be in JobManagerView, shouldn't it? */
	Glib::signal_timeout().connect (sigc::bind_return (sigc::mem_fun (jobs_view, &JobManagerView::update), true), 1000);

	window->maximize ();
	Gtk::Main::run (*window);

	return 0;
}
