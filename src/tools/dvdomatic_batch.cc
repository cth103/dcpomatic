/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/aboutdlg.h>
#include <wx/wx.h>
#include "lib/version.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/util.h"
#include "wx/wx_util.h"
#include "wx/wx_ui_signaller.h"
#include "wx/batch_view.h"

enum {
	ID_file_quit = 1,
	ID_help_about
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	file->Append (ID_file_quit, _("&Quit"));

	wxMenu* help = new wxMenu;
	help->Append (ID_help_about, _("About"));

	m->Append (file, _("&File"));
	m->Append (help, _("&Help"));
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

		Connect (ID_file_quit, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_quit));
		Connect (ID_help_about, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::help_about));

		wxPanel* panel = new wxPanel (this);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		wxSizer* sizer = new wxBoxSizer (wxVERTICAL);

		BatchView* batch_view = new BatchView (panel);
		sizer->Add (batch_view, 1, wxALL | wxEXPAND, 6);

		wxSizer* buttons = new wxBoxSizer (wxHORIZONTAL);
		wxButton* add = new wxButton (panel, wxID_ANY, _("Add Film..."));
		buttons->Add (add, 1, wxALL, 6);
		wxButton* start = new wxButton (panel, wxID_ANY, _("Start..."));
		buttons->Add (start, 1, wxALL, 6);

		sizer->Add (buttons, 0, wxALL, 6);

		panel->SetSizer (sizer);
	}

private:
	void file_quit (wxCommandEvent &)
	{
		Close (true);
	}

	void help_about (wxCommandEvent &)
	{
		wxAboutDialogInfo info;
		info.SetName (_("DVD-o-matic Batch Converter"));
		if (strcmp (dvdomatic_git_commit, "release") == 0) {
			info.SetVersion (std_to_wx (String::compose ("version %1", dvdomatic_version)));
		} else {
			info.SetVersion (std_to_wx (String::compose ("version %1 git %2", dvdomatic_version, dvdomatic_git_commit)));
		}
		info.SetDescription (_("Free, open-source DCP generation from almost anything."));
		info.SetCopyright (_("(C) 2012-2013 Carl Hetherington, Terrence Meiczinger, Paul Davis, Ole Laursen"));

		wxArrayString authors;
		authors.Add (wxT ("Carl Hetherington"));
		authors.Add (wxT ("Terrence Meiczinger"));
		authors.Add (wxT ("Paul Davis"));
		authors.Add (wxT ("Ole Laursen"));
		info.SetDevelopers (authors);

		wxArrayString translators;
		translators.Add (wxT ("Olivier Perriere"));
		translators.Add (wxT ("Lilian Lefranc"));
		translators.Add (wxT ("Thierry Journet"));
		translators.Add (wxT ("Massimiliano Broggi"));
		translators.Add (wxT ("Manuel AC"));
		translators.Add (wxT ("Adam Klotblixt"));
		info.SetTranslators (translators);
		
		info.SetWebSite (wxT ("http://carlh.net/software/dvdomatic"));
		wxAboutBox (info);
	}
};

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DVDOMATIC_POSIX		
		unsetenv ("UBUNTU_MENUPROXY");
#endif		

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dvdomatic_setup
		   hasn't yet been called and there aren't any scalers, filters etc.
		   set up yet.
		*/
		dvdomatic_setup_i18n ();

		/* Set things up, including scalers / filters etc.
		   which will now be internationalised correctly.
		*/
		dvdomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		Frame* f = new Frame (_("DVD-o-matic Batch Converter"));
		SetTopWindow (f);
		f->Maximize ();
		f->Show ();

		ui_signaller = new wxUISignaller (this);
		this->Connect (-1, wxEVT_IDLE, wxIdleEventHandler (App::idle));

		return true;
	}

	void idle (wxIdleEvent &)
	{
		ui_signaller->ui_idle ();
	}
};

IMPLEMENT_APP (App)
