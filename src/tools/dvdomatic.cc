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
#include <wx/aboutdlg.h>
#include "wx/film_viewer.h"
#include "wx/film_editor.h"
#ifndef DVDOMATIC_DISABLE_PLAYER
#include "wx/film_player.h"
#endif
#include "wx/job_manager_view.h"
//#include "gtk/config_dialog.h"
#include "wx/job_wrapper.h"
//#include "gtk/dvd_title_dialog.h"
#include "wx/wx_util.h"
#include "lib/film.h"
#include "lib/format.h"
#include "lib/config.h"
#include "lib/filter.h"
#include "lib/util.h"
#include "lib/scaler.h"
#include "lib/exceptions.h"

using namespace std;
using namespace boost;

static wxFrame* frame = 0;
static FilmEditor* film_editor = 0;
static FilmViewer* film_viewer = 0;

#ifndef DVDOMATIC_DISABLE_PLAYER
static FilmPlayer* film_player = 0;
#endif
static Film* film = 0;

static void set_menu_sensitivity ();

class FilmChangedDialog
{
public:
	FilmChangedDialog ()
	{
		stringstream s;
		s << "Save changes to film \"" << film->name() << "\" before closing?";
		_dialog = new wxMessageDialog (frame, std_to_wx (s.str()), wxT ("Film changed"), wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
	}

	~FilmChangedDialog ()
	{
		_dialog->Destroy ();
	}

	int run ()
	{
		return _dialog->ShowModal ();
	}

private:	
	wxMessageDialog* _dialog;
};

void
maybe_save_then_delete_film ()
{
	if (!film) {
		return;
	}
			
	if (film->dirty ()) {
		FilmChangedDialog d;
		switch (d.run ()) {
		case wxID_NO:
			break;
		case wxID_YES:
			film->write_metadata ();
			break;
		}
	}
	
	delete film;
	film = 0;
}



enum Sensitivity {
	ALWAYS,
	NEEDS_FILM
};

map<wxMenuItem*, Sensitivity> menu_items;
	
void
add_item (wxMenu* menu, std::string text, int id, Sensitivity sens)
{
	wxMenuItem* item = menu->Append (id, std_to_wx (text));
	menu_items.insert (make_pair (item, sens));
}

void
set_menu_sensitivity ()
{
	for (map<wxMenuItem*, Sensitivity>::iterator i = menu_items.begin(); i != menu_items.end(); ++i) {
		if (i->second == NEEDS_FILM) {
			i->first->Enable (film != 0);
		} else {
			i->first->Enable (true);
		}
	}
}

enum {
	ID_file_new = 1,
	ID_file_open,
	ID_file_save,
	ID_file_quit,
	ID_edit_preferences,
	ID_jobs_make_dcp,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_copy_from_dvd,
	ID_jobs_examine_content,
	ID_jobs_make_dcp_from_existing_transcode,
	ID_help_about
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	add_item (file, "New...", ID_file_new, ALWAYS);
	add_item (file, "&Open...", ID_file_open, ALWAYS);
	file->AppendSeparator ();
	add_item (file, "&Save", ID_file_save, NEEDS_FILM);
	file->AppendSeparator ();
	add_item (file, "&Quit", ID_file_quit, ALWAYS);

	wxMenu* edit = new wxMenu;
	add_item (edit, "&Preferences...", ID_edit_preferences, ALWAYS);

	wxMenu* jobs = new wxMenu;
	add_item (jobs, "&Make DCP", ID_jobs_make_dcp, NEEDS_FILM);
	add_item (jobs, "&Send DCP to TMS", ID_jobs_send_dcp_to_tms, NEEDS_FILM);
	add_item (jobs, "Copy from &DVD...", ID_jobs_copy_from_dvd, NEEDS_FILM);
	jobs->AppendSeparator ();
	add_item (jobs, "&Examine content", ID_jobs_examine_content, NEEDS_FILM);
	add_item (jobs, "Make DCP from existing &transcode", ID_jobs_make_dcp_from_existing_transcode, NEEDS_FILM);

	wxMenu* help = new wxMenu;
	add_item (help, "About", ID_help_about, ALWAYS);

	m->Append (file, _("&File"));
	m->Append (edit, _("&Edit"));
	m->Append (jobs, _("&Jobs"));
	m->Append (help, _("&Help"));
}

bool
window_closed (wxCommandEvent &)
{
	maybe_save_then_delete_film ();
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
	
	frame->SetTitle (std_to_wx (s.str()));
}

class Frame : public wxFrame
{
public:
	Frame (wxString const & title)
		: wxFrame (NULL, -1, title)
	{
		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);
	}

	void file_new (wxCommandEvent &)
	{
		wxDirDialog* c = new wxDirDialog (frame, wxT ("New Film"));
		int const r = c->ShowModal ();
		c->Destroy ();
		
		if (r == wxID_OK) {
			maybe_save_then_delete_film ();
			film = new Film (wx_to_std (c->GetPath ()));
#if BOOST_FILESYSTEM_VERSION == 3		
			film->set_name (filesystem::path (wx_to_std (c->GetPath())).filename().generic_string());
#else		
			film->set_name (filesystem::path (wx_to_std (c->GetPath())).filename());
#endif		
			film_viewer->set_film (film);
			film_editor->set_film (film);
			set_menu_sensitivity ();
		}
	}

	void file_open (wxCommandEvent &)
	{
		wxDirDialog* c = new wxDirDialog (frame, wxT ("Open Film"), wxT (""), wxDD_DIR_MUST_EXIST);
		int const r = c->ShowModal ();
		c->Destroy ();
		
		if (r == wxID_OK) {
			maybe_save_then_delete_film ();
			film = new Film (wx_to_std (c->GetPath ()));
			film_viewer->set_film (film);
			film_editor->set_film (film);
			set_menu_sensitivity ();
		}
	}

	void file_save (wxCommandEvent &)
	{
		film->write_metadata ();
	}
	
	void file_quit (wxCommandEvent &)
	{
		maybe_save_then_delete_film ();
		frame->Close (true);
	}

	void edit_preferences (wxCommandEvent &)
	{
//	ConfigDialog d;
//	d.run ();
//	Config::instance()->write ();
	}

	void jobs_make_dcp (wxCommandEvent &)
	{
		JobWrapper::make_dcp (this, film, true);
	}
	
	void jobs_make_dcp_from_existing_transcode (wxCommandEvent &)
	{
		JobWrapper::make_dcp (this, film, false);
	}
	
	void jobs_copy_from_dvd (wxCommandEvent &)
	{
//	try {
//		DVDTitleDialog d;
//		if (d.run () != Gtk::RESPONSE_OK) {
//			return;
//		}
//		film->copy_from_dvd ();
//	} catch (DVDError& e) {
//		error_dialog (e.what ());
//	}
	}
	
	void jobs_send_dcp_to_tms (wxCommandEvent &)
	{
		film->send_dcp_to_tms ();
	}
	
	void jobs_examine_content (wxCommandEvent &)
	{
		film->examine_content ();
	}
	
	void help_about (wxCommandEvent &)
	{
		wxAboutDialogInfo info;
		info.SetName (_("DVD-o-matic"));
		info.SetVersion (wxT (DVDOMATIC_VERSION));
		info.SetDescription (_("DCP generation from arbitrary formats"));
		info.SetCopyright (_("(C) Carl Hetherington, Terrence Meiczinger, Paul Davis"));
		wxArrayString authors;
		authors.Add (wxT ("Carl Hetherington"));
		authors.Add (wxT ("Terrence Meiczinger"));
		authors.Add (wxT ("Paul Davis"));
		info.SetDevelopers (authors);
		info.SetWebSite (wxT ("http://carlh.net/software/dvdomatic"));
		wxAboutBox (info);
	}
};

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit ()) {
			return false;
		}

		wxInitAllImageHandlers ();
		
		dvdomatic_setup ();

//		if (argc == 2 && boost::filesystem::is_directory (argv[1])) {
//			film = new Film (argv[1]);
//		}
		
		frame = new Frame (_("DVD-o-matic"));
		frame->Show (true);
		
		frame->Connect (ID_file_new, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_new));
		frame->Connect (ID_file_open, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_open));
		frame->Connect (ID_file_save, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_save));
		frame->Connect (ID_file_quit, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_quit));
		frame->Connect (ID_edit_preferences, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::edit_preferences));
		frame->Connect (ID_jobs_make_dcp, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_make_dcp));
		frame->Connect (ID_jobs_send_dcp_to_tms, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_send_dcp_to_tms));
		frame->Connect (ID_jobs_copy_from_dvd, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_copy_from_dvd));
		frame->Connect (ID_jobs_examine_content, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_examine_content));
		frame->Connect (ID_jobs_make_dcp_from_existing_transcode, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_make_dcp_from_existing_transcode));
		frame->Connect (ID_help_about, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::help_about));

		wxPanel* rhs = new wxPanel (frame);
		
		film_editor = new FilmEditor (film, frame);
		film_viewer = new FilmViewer (film, rhs);
#ifndef DVDOMATIC_DISABLE_PLAYER
		film_player = new FilmPlayer (film, frame);
#endif
		JobManagerView* job_manager_view = new JobManagerView (rhs);

		wxSizer* rhs_sizer = new wxBoxSizer (wxVERTICAL);
		rhs_sizer->Add (film_viewer, 3, wxEXPAND | wxALL);
		rhs_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL);
		rhs->SetSizer (rhs_sizer);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (film_editor, 0, wxALL, 6);
		main_sizer->Add (rhs, 1, wxEXPAND | wxALL, 6);
		frame->SetSizer (main_sizer);

		SetTopWindow (frame);
		frame->Maximize ();

		set_menu_sensitivity ();

#if 0
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
#endif		
		
		return true;
	}
};

IMPLEMENT_APP (App)
