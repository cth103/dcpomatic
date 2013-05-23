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
#include <wx/stdpaths.h>
#include <wx/wx.h>
#include "lib/version.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_ui_signaller.h"
#include "wx/job_manager_view.h"

using boost::shared_ptr;

enum {
	ID_file_add_film = 1,
	ID_file_quit,
	ID_help_about
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	file->Append (ID_file_add_film, _("&Add Film..."));
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

		Connect (ID_file_add_film, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_add_film));
		Connect (ID_file_quit, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_quit));
		Connect (ID_help_about, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::help_about));

		wxPanel* panel = new wxPanel (this);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		wxSizer* sizer = new wxBoxSizer (wxVERTICAL);

		JobManagerView* job_manager_view = new JobManagerView (panel, JobManagerView::PAUSE);
		sizer->Add (job_manager_view, 1, wxALL | wxEXPAND, 6);

		wxSizer* buttons = new wxBoxSizer (wxHORIZONTAL);
		wxButton* add = new wxButton (panel, wxID_ANY, _("Add Film..."));
		add->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (Frame::add_film));
		buttons->Add (add, 1, wxALL, 6);

		sizer->Add (buttons, 0, wxALL, 6);

		panel->SetSizer (sizer);

		Connect (wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler (Frame::close));
	}

private:
	bool should_close ()
	{
		if (!JobManager::instance()->work_to_do ()) {
			return true;
		}

		wxMessageDialog* d = new wxMessageDialog (
			0,
			_("There are unfinished jobs; are you sure you want to quit?"),
			_("Unfinished jobs"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		bool const r = d->ShowModal() == wxID_YES;
		d->Destroy ();
		return r;
	}
		
	void close (wxCloseEvent& ev)
	{
		if (!should_close ()) {
			ev.Veto ();
			return;
		}

		ev.Skip ();
	}

	void file_add_film (wxCommandEvent& ev)
	{
		add_film (ev);
	}
	
	void file_quit (wxCommandEvent &)
	{
		if (should_close ()) {
			Close (true);
		}
	}

	void help_about (wxCommandEvent &)
	{
		wxAboutDialogInfo info;
		info.SetName (_("DCP-o-matic Batch Converter"));
		if (strcmp (dcpomatic_git_commit, "release") == 0) {
			info.SetVersion (std_to_wx (String::compose ("version %1", dcpomatic_version)));
		} else {
			info.SetVersion (std_to_wx (String::compose ("version %1 git %2", dcpomatic_version, dcpomatic_git_commit)));
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
		
		info.SetWebSite (wxT ("http://carlh.net/software/dcpomatic"));
		wxAboutBox (info);
	}

	void add_film (wxCommandEvent &)
	{
		wxDirDialog* c = new wxDirDialog (this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		int r;
		while (1) {
			r = c->ShowModal ();
			if (r == wxID_OK && c->GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}
			
		if (r == wxID_OK) {
			try {
				shared_ptr<Film> film (new Film (wx_to_std (c->GetPath ())));
				film->read_metadata ();
				film->make_dcp ();
			} catch (std::exception& e) {
				wxString p = c->GetPath ();
				wxCharBuffer b = p.ToUTF8 ();
				error_dialog (this, wxString::Format (_("Could not open film at %s (%s)"), p.data(), std_to_wx (e.what()).data()));
			}
		}

		c->Destroy ();
	}
};

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DCPOMATIC_POSIX		
		unsetenv ("UBUNTU_MENUPROXY");
#endif		

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any scalers, filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including scalers / filters etc.
		   which will now be internationalised correctly.
		*/
		dcpomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		Frame* f = new Frame (_("DCP-o-matic Batch Converter"));
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
