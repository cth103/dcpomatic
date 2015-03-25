/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
#include <wx/cmdline.h>
#include <wx/wx.h>
#include "lib/version.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "wx/wx_util.h"
#include "wx/about_dialog.h"
#include "wx/wx_ui_signaller.h"
#include "wx/job_manager_view.h"

using std::exception;
using boost::shared_ptr;

static std::string film_to_load;

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
		, _sizer (new wxBoxSizer (wxVERTICAL))
	{
		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_add_film, this), ID_file_add_film);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_quit, this),     ID_file_quit);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::help_about, this),    ID_help_about);

		wxPanel* panel = new wxPanel (this);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		JobManagerView* job_manager_view = new JobManagerView (panel);
		_sizer->Add (job_manager_view, 1, wxALL | wxEXPAND, 6);

		wxSizer* buttons = new wxBoxSizer (wxHORIZONTAL);
		wxButton* add = new wxButton (panel, wxID_ANY, _("Add Film..."));
		add->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&Frame::add_film, this));
		buttons->Add (add, 1, wxALL, 6);

		_sizer->Add (buttons, 0, wxALL, 6);

		panel->SetSizer (_sizer);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&Frame::close, this, _1));
		Bind (wxEVT_SIZE, boost::bind (&Frame::sized, this, _1));
	}

private:
	void sized (wxSizeEvent& ev)
	{
		_sizer->Layout ();
		ev.Skip ();
	}
	
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

	void file_add_film ()
	{
		add_film ();
	}
	
	void file_quit ()
	{
		if (should_close ()) {
			Close (true);
		}
	}

	void help_about ()
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void add_film ()
	{
		wxDirDialog* c = new wxDirDialog (this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		if (_last_parent) {
			c->SetPath (std_to_wx (_last_parent.get().string ()));
		}
		
		int r;
		while (true) {
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

		_last_parent = boost::filesystem::path (wx_to_std (c->GetPath ())).parent_path ();

		c->Destroy ();
	}

	boost::optional<boost::filesystem::path> _last_parent;
	wxSizer* _sizer;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "film to load", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DCPOMATIC_LINUX		
		unsetenv ("UBUNTU_MENUPROXY");
#endif		

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including filters etc.
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
		this->Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		shared_ptr<Film> film;
		if (!film_to_load.empty() && boost::filesystem::is_directory (film_to_load)) {
			try {
				film.reset (new Film (film_to_load));
				film->read_metadata ();
				film->make_dcp ();
			} catch (exception& e) {
				error_dialog (0, std_to_wx (String::compose (wx_to_std (_("Could not load film %1 (%2)")), film_to_load, e.what())));
			}
		}

		return true;
	}

	void idle ()
	{
		ui_signaller->ui_idle ();
	}

	void OnInitCmdLine (wxCmdLineParser& parser)
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser)
	{
		if (parser.GetParamCount() > 0) {
			film_to_load = wx_to_std (parser.GetParam(0));
		}

		return true;
	}
};

IMPLEMENT_APP (App)
