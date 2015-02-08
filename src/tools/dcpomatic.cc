/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/ui_signaller.h"
#include "lib/log.h"
#include "lib/job_manager.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/cinema.h"
#include "lib/kdm.h"
#include "lib/send_kdm_email_job.h"
#include "lib/server_finder.h"
#include "lib/update.h"
#include "lib/content_factory.h"
#include "wx/film_viewer.h"
#include "wx/film_editor.h"
#include "wx/job_manager_view.h"
#include "wx/config_dialog.h"
#include "wx/wx_util.h"
#include "wx/new_film_dialog.h"
#include "wx/properties_dialog.h"
#include "wx/wx_ui_signaller.h"
#include "wx/about_dialog.h"
#include "wx/kdm_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/hints_dialog.h"
#include "wx/update_dialog.h"
#include "wx/content_panel.h"
#include "wx/report_problem_dialog.h"
#include <dcp/exceptions.h>
#include <wx/generic/aboutdlgg.h>
#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include <wx/preferences.h>
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef check
#undef check
#endif

using std::cout;
using std::wcout;
using std::string;
using std::vector;
using std::wstring;
using std::wstringstream;
using std::map;
using std::make_pair;
using std::list;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

class FilmChangedDialog
{
public:
	FilmChangedDialog (string name)
	{
		_dialog = new wxMessageDialog (
			0,
			wxString::Format (_("Save changes to film \"%s\" before closing?"), std_to_wx (name).data()),
			/* TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			   project (Film) has been changed since it was last saved.
			*/
			_("Film changed"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);
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
	/* Not defined */
	FilmChangedDialog (FilmChangedDialog const &);
	
	wxMessageDialog* _dialog;
};

#define ALWAYS                       0x0
#define NEEDS_FILM                   0x1
#define NOT_DURING_DCP_CREATION      0x2
#define NEEDS_CPL                    0x4
#define NEEDS_SELECTED_VIDEO_CONTENT 0x8

map<wxMenuItem*, int> menu_items;
	
enum {
	ID_file_new = 1,
	ID_file_open,
	ID_file_save,
	ID_file_properties,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_content_scale_to_fit_width = 100,
	ID_content_scale_to_fit_height,
	ID_jobs_make_dcp,
	ID_jobs_make_kdms,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_show_dcp,
	ID_tools_hints,
	ID_tools_encoding_servers,
	ID_tools_check_for_updates,
	ID_help_report_a_problem,
	/* IDs for shortcuts (with no associated menu item) */
	ID_add_file
};

class Frame : public wxFrame
{
public:
	Frame (wxString const & title)
		: wxFrame (NULL, -1, title)
		, _hints_dialog (0)
		, _servers_list_dialog (0)
		, _config_dialog (0)
		, _file_menu (0)
		, _history_items (0)
		, _history_position (0)
		, _history_separator (0)
	{
#if defined(DCPOMATIC_WINDOWS)
		if (Config::instance()->win32_console ()) {
			AllocConsole();
			
			HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
			int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
			FILE* hf_out = _fdopen(hCrt, "w");
			setvbuf(hf_out, NULL, _IONBF, 1);
			*stdout = *hf_out;
			
			HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
			hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
			FILE* hf_in = _fdopen(hCrt, "r");
			setvbuf(hf_in, NULL, _IONBF, 128);
			*stdin = *hf_in;

			cout << "DCP-o-matic is starting." << "\n";
		}
#endif

		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		_config_changed_connection = Config::instance()->Changed.connect (boost::bind (&Frame::config_changed, this));
		config_changed ();

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_new, this),                ID_file_new);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_open, this),               ID_file_open);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_save, this),               ID_file_save);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_properties, this),         ID_file_properties);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_history, this, _1),        ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_exit, this),               wxID_EXIT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::edit_preferences, this),        wxID_PREFERENCES);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::content_scale_to_fit_width, this), ID_content_scale_to_fit_width);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::content_scale_to_fit_height, this), ID_content_scale_to_fit_height);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_dcp, this),           ID_jobs_make_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_kdms, this),          ID_jobs_make_kdms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_send_dcp_to_tms, this),    ID_jobs_send_dcp_to_tms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_show_dcp, this),           ID_jobs_show_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::tools_hints, this),             ID_tools_hints);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::tools_encoding_servers, this),  ID_tools_encoding_servers);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::tools_check_for_updates, this), ID_tools_check_for_updates);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::help_about, this),              wxID_ABOUT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::help_report_a_problem, this),   ID_help_report_a_problem);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&Frame::close, this, _1));

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);

		_film_editor = new FilmEditor (overall_panel);
		_film_viewer = new FilmViewer (overall_panel);
		JobManagerView* job_manager_view = new JobManagerView (overall_panel);

		wxBoxSizer* right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add (_film_viewer, 2, wxEXPAND | wxALL, 6);
		right_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL, 6);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (_film_editor, 1, wxEXPAND | wxALL, 6);
		main_sizer->Add (right_sizer, 2, wxEXPAND | wxALL, 6);

		set_menu_sensitivity ();

		_film_editor->FileChanged.connect (bind (&Frame::file_changed, this, _1));
		file_changed ("");

		JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&Frame::set_menu_sensitivity, this));

		overall_panel->SetSizer (main_sizer);

		wxAcceleratorEntry accel[1];
		accel[0].Set (wxACCEL_CTRL, static_cast<int>('A'), ID_add_file);
		Bind (wxEVT_MENU, boost::bind (&ContentPanel::add_file_clicked, _film_editor->content_panel()), ID_add_file);
		wxAcceleratorTable accel_table (1, accel);
		SetAcceleratorTable (accel_table);
	}

	void new_film (boost::filesystem::path path)
	{
		shared_ptr<Film> film (new Film (path));
		film->write_metadata ();
		film->set_name (path.filename().generic_string());
		set_film (film);
	}

	void load_film (boost::filesystem::path file)
	try
	{
		maybe_save_then_delete_film ();
		
		shared_ptr<Film> film (new Film (file));
		list<string> const notes = film->read_metadata ();

		if (film->state_version() == 4) {
			error_dialog (
				0,
				_("This film was created with an old version of DVD-o-matic and may not load correctly "
				  "in this version.  Please check the film's settings carefully.")
				);
		}
		
		for (list<string>::const_iterator i = notes.begin(); i != notes.end(); ++i) {
			error_dialog (0, std_to_wx (*i));
		}
		
		set_film (film);
	}
	catch (std::exception& e) {
		wxString p = std_to_wx (file.string ());
		wxCharBuffer b = p.ToUTF8 ();
		error_dialog (this, wxString::Format (_("Could not open film at %s (%s)"), p.data(), std_to_wx (e.what()).data()));
	}

	void set_film (shared_ptr<Film> film)
	{
		_film = film;
		_film_viewer->set_film (_film);
		_film_editor->set_film (_film);
		set_menu_sensitivity ();
		Config::instance()->add_to_history (_film->directory ());
	}

	shared_ptr<Film> film () const {
		return _film;
	}
	
private:

	void file_changed (boost::filesystem::path f)
	{
		string s = wx_to_std (_("DCP-o-matic"));
		if (!f.empty ()) {
			s += " - " + f.string ();
		}
		
		SetTitle (std_to_wx (s));
	}
	
	void file_new ()
	{
		NewFilmDialog* d = new NewFilmDialog (this);
		int const r = d->ShowModal ();
		
		if (r == wxID_OK) {

			if (boost::filesystem::is_directory (d->get_path()) && !boost::filesystem::is_empty(d->get_path())) {
				if (!confirm_dialog (
					    this,
					    std_to_wx (
						    String::compose (wx_to_std (_("The directory %1 already exists and is not empty.  "
										  "Are you sure you want to use it?")),
								     d->get_path().string().c_str())
						    )
					    )) {
					return;
				}
			} else if (boost::filesystem::is_regular_file (d->get_path())) {
				error_dialog (
					this,
					String::compose (wx_to_std (_("%1 already exists as a file, so you cannot use it for a new film.")), d->get_path().c_str())
					);
				return;
			}
			
			maybe_save_then_delete_film ();
			new_film (d->get_path ());
		}
		
		d->Destroy ();
	}

	void file_open ()
	{
		wxDirDialog* c = new wxDirDialog (
			this,
			_("Select film to open"),
			std_to_wx (Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()),
			wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST
			);
		
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
			load_film (wx_to_std (c->GetPath ()));
		}

		c->Destroy ();
	}

	void file_save ()
	{
		_film->write_metadata ();
	}

	void file_properties ()
	{
		PropertiesDialog* d = new PropertiesDialog (this, _film);
		d->ShowModal ();
		d->Destroy ();
	}

	void file_history (wxCommandEvent& event)
	{
		vector<boost::filesystem::path> history = Config::instance()->history ();
		int n = event.GetId() - ID_file_history;
		if (n >= 0 && n < static_cast<int> (history.size ())) {
			load_film (history[n]);
		}
	}
	
	void file_exit ()
	{
		/* false here allows the close handler to veto the close request */
		Close (false);
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void jobs_make_dcp ()
	{
		double required;
		double available;

		if (!_film->should_be_enough_disk_space (required, available)) {
			if (!confirm_dialog (this, wxString::Format (_("The DCP for this film will take up about %.1f Gb, and the disk that you are using only has %.1f Gb available.  Do you want to continue anyway?"), required, available))) {
				return;
			}
		}

		try {
			/* It seems to make sense to auto-save metadata here, since the make DCP may last
			   a long time, and crashes/power failures are moderately likely.
			*/
			_film->write_metadata ();
			_film->make_dcp ();
		} catch (BadSettingError& e) {
			error_dialog (this, wxString::Format (_("Bad setting for %s (%s)"), std_to_wx(e.setting()).data(), std_to_wx(e.what()).data()));
		} catch (std::exception& e) {
			error_dialog (this, wxString::Format (_("Could not make DCP: %s"), std_to_wx(e.what()).data()));
		}
	}

	void jobs_make_kdms ()
	{
		if (!_film) {
			return;
		}
		
		KDMDialog* d = new KDMDialog (this, _film);
		if (d->ShowModal () != wxID_OK) {
			d->Destroy ();
			return;
		}

		try {
			if (d->write_to ()) {
				write_kdm_files (_film, d->screens (), d->cpl (), d->from (), d->until (), d->formulation (), d->directory ());
			} else {
				JobManager::instance()->add (
					shared_ptr<Job> (new SendKDMEmailJob (_film, d->screens (), d->cpl (), d->from (), d->until (), d->formulation ()))
					);
			}
		} catch (dcp::NotEncryptedError& e) {
			error_dialog (this, _("CPL's content is not encrypted."));
		} catch (exception& e) {
			error_dialog (this, e.what ());
		} catch (...) {
			error_dialog (this, _("An unknown exception occurred."));
		}
	
		d->Destroy ();
	}

	void content_scale_to_fit_width ()
	{
		VideoContentList vc = _film_editor->content_panel()->selected_video ();
		for (VideoContentList::iterator i = vc.begin(); i != vc.end(); ++i) {
			(*i)->scale_and_crop_to_fit_width ();
		}
	}

	void content_scale_to_fit_height ()
	{
		VideoContentList vc = _film_editor->content_panel()->selected_video ();
		for (VideoContentList::iterator i = vc.begin(); i != vc.end(); ++i) {
			(*i)->scale_and_crop_to_fit_height ();
		}
	}
	
	void jobs_send_dcp_to_tms ()
	{
		_film->send_dcp_to_tms ();
	}

	void jobs_show_dcp ()
	{
#ifdef DCPOMATIC_WINDOWS
		wstringstream args;
		args << "/select," << _film->dir (_film->dcp_name(false));
		ShellExecute (0, L"open", L"explorer.exe", args.str().c_str(), 0, SW_SHOWDEFAULT);
#endif

#ifdef DCPOMATIC_LINUX
		int r = system ("which nautilus");
		if (WEXITSTATUS (r) == 0) {
			r = system (string ("nautilus " + _film->directory().string()).c_str ());
			if (WEXITSTATUS (r)) {
				error_dialog (this, _("Could not show DCP (could not run nautilus)"));
			}
		} else {
			int r = system ("which konqueror");
			if (WEXITSTATUS (r) == 0) {
				r = system (string ("konqueror " + _film->directory().string()).c_str ());
				if (WEXITSTATUS (r)) {
					error_dialog (this, _("Could not show DCP (could not run konqueror)"));
				}
			}
		}
#endif		

#ifdef DCPOMATIC_OSX
		int r = system (string ("open -R " + _film->dir (_film->dcp_name (false)).string ()).c_str ());
		if (WEXITSTATUS (r)) {
			error_dialog (this, _("Could not show DCP"));
		}
#endif		       
	}

	void tools_hints ()
	{
		if (!_hints_dialog) {
			_hints_dialog = new HintsDialog (this, _film);
		}

		_hints_dialog->Show ();
	}

	void tools_encoding_servers ()
	{
		if (!_servers_list_dialog) {
			_servers_list_dialog = new ServersListDialog (this);
		}

		_servers_list_dialog->Show ();
	}

	void tools_check_for_updates ()
	{
		UpdateChecker::instance()->run ();
	}

	void help_about ()
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog* d = new ReportProblemDialog (this, _film);
		if (d->ShowModal () == wxID_OK) {
			d->report ();
		}
		d->Destroy ();
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

		/* We don't want to hear about any more configuration changes, since they
		   cause the File menu to be altered, which itself will be deleted around
		   now (without, as far as I can see, any way for us to find out).
		*/
		_config_changed_connection.disconnect ();
		
		maybe_save_then_delete_film ();
		ev.Skip ();
	}

	void set_menu_sensitivity ()
	{
		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();
		list<shared_ptr<Job> >::iterator i = jobs.begin();
		while (i != jobs.end() && dynamic_pointer_cast<TranscodeJob> (*i) == 0) {
			++i;
		}
		bool const dcp_creation = (i != jobs.end ()) && !(*i)->finished ();
		bool const have_cpl = _film && !_film->cpls().empty ();
		bool const have_selected_video_content = !_film_editor->content_panel()->selected_video().empty();
		
		for (map<wxMenuItem*, int>::iterator j = menu_items.begin(); j != menu_items.end(); ++j) {
			
			bool enabled = true;
			
			if ((j->second & NEEDS_FILM) && !_film) {
				enabled = false;
			}
			
			if ((j->second & NOT_DURING_DCP_CREATION) && dcp_creation) {
				enabled = false;
			}
			
			if ((j->second & NEEDS_CPL) && !have_cpl) {
				enabled = false;
			}
			
			if ((j->second & NEEDS_SELECTED_VIDEO_CONTENT) && !have_selected_video_content) {
				enabled = false;
			}
			
			j->first->Enable (enabled);
		}
	}

	void maybe_save_then_delete_film ()
	{
		if (!_film) {
			return;
		}
		
		if (_film->dirty ()) {
			FilmChangedDialog d (_film->name ());
			switch (d.run ()) {
			case wxID_NO:
				break;
			case wxID_YES:
				_film->write_metadata ();
				break;
			}
		}
		
		_film.reset ();
	}

	void add_item (wxMenu* menu, wxString text, int id, int sens)
	{
		wxMenuItem* item = menu->Append (id, text);
		menu_items.insert (make_pair (item, sens));
	}
	
	void setup_menu (wxMenuBar* m)
	{
		_file_menu = new wxMenu;
		add_item (_file_menu, _("New...\tCtrl-N"), ID_file_new, ALWAYS);
		add_item (_file_menu, _("&Open...\tCtrl-O"), ID_file_open, ALWAYS);
		_file_menu->AppendSeparator ();
		add_item (_file_menu, _("&Save\tCtrl-S"), ID_file_save, NEEDS_FILM);
		_file_menu->AppendSeparator ();
		add_item (_file_menu, _("&Properties..."), ID_file_properties, NEEDS_FILM);

		_history_position = _file_menu->GetMenuItems().GetCount();

#ifndef __WXOSX__	
		_file_menu->AppendSeparator ();
#endif
	
#ifdef __WXOSX__	
		add_item (_file_menu, _("&Exit"), wxID_EXIT, ALWAYS);
#else
		add_item (_file_menu, _("&Quit"), wxID_EXIT, ALWAYS);
#endif	
	
#ifdef __WXOSX__	
		add_item (_file_menu, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#else
		wxMenu* edit = new wxMenu;
		add_item (edit, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#endif

		wxMenu* content = new wxMenu;
		add_item (content, _("Scale to fit &width"), ID_content_scale_to_fit_width, NEEDS_FILM | NEEDS_SELECTED_VIDEO_CONTENT);
		add_item (content, _("Scale to fit &height"), ID_content_scale_to_fit_height, NEEDS_FILM | NEEDS_SELECTED_VIDEO_CONTENT);
		
		wxMenu* jobs_menu = new wxMenu;
		add_item (jobs_menu, _("&Make DCP\tCtrl-M"), ID_jobs_make_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		add_item (jobs_menu, _("Make &KDMs...\tCtrl-K"), ID_jobs_make_kdms, NEEDS_FILM);
		add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);
		add_item (jobs_menu, _("S&how DCP"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);

		wxMenu* tools = new wxMenu;
		add_item (tools, _("Hints...\tCtrl-H"), ID_tools_hints, 0);
		add_item (tools, _("Encoding servers..."), ID_tools_encoding_servers, 0);
		add_item (tools, _("Check for updates"), ID_tools_check_for_updates, 0);
		
		wxMenu* help = new wxMenu;
#ifdef __WXOSX__	
		add_item (help, _("About DCP-o-matic"), wxID_ABOUT, ALWAYS);
#else	
		add_item (help, _("About"), wxID_ABOUT, ALWAYS);
#endif	
		add_item (help, _("Report a problem..."), ID_help_report_a_problem, ALWAYS);
		
		m->Append (_file_menu, _("&File"));
#ifndef __WXOSX__	
		m->Append (edit, _("&Edit"));
#endif
		m->Append (content, _("&Content"));
		m->Append (jobs_menu, _("&Jobs"));
		m->Append (tools, _("&Tools"));
		m->Append (help, _("&Help"));
	}

	void config_changed ()
	{
		for (int i = 0; i < _history_items; ++i) {
			delete _file_menu->Remove (ID_file_history + i);
		}

		if (_history_separator) {
			_file_menu->Remove (_history_separator);
		}
		delete _history_separator;
		_history_separator = 0;
		
		int pos = _history_position;
		
		vector<boost::filesystem::path> history = Config::instance()->history ();
		
		if (!history.empty ()) {
			_history_separator = _file_menu->InsertSeparator (pos++);
		}
		
		for (size_t i = 0; i < history.size(); ++i) {
			SafeStringStream s;
			if (i < 9) {
				s << "&" << (i + 1) << " ";
			}
			s << history[i].string();
			_file_menu->Insert (pos++, ID_file_history + i, std_to_wx (s.str ()));
		}

		_history_items = history.size ();
	}
	
	FilmEditor* _film_editor;
	FilmViewer* _film_viewer;
	HintsDialog* _hints_dialog;
	ServersListDialog* _servers_list_dialog;
	wxPreferencesEditor* _config_dialog;
	wxMenu* _file_menu;
	shared_ptr<Film> _film;
	int _history_items;
	int _history_position;
	wxMenuItem* _history_separator;
	boost::signals2::scoped_connection _config_changed_connection;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_SWITCH, "n", "new", "create new film", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "content", "add content file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_PARAM, 0, 0, "film to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (0)
	{}

private:	
		
	bool OnInit ()
	try
	{
		SetAppName (_("DCP-o-matic"));
		
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DCPOMATIC_LINUX	
		unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef __WXOSX__		
		ProcessSerialNumber serial;
		GetCurrentProcess (&serial);
		TransformProcessType (&serial, kProcessTransformToForegroundApplication);
#endif		

		wxInitAllImageHandlers ();

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

		_frame = new Frame (_("DCP-o-matic"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		_frame->Show ();

		if (!_film_to_load.empty() && boost::filesystem::is_directory (_film_to_load)) {
			try {
				_frame->load_film (_film_to_load);
			} catch (exception& e) {
				error_dialog (0, std_to_wx (String::compose (wx_to_std (_("Could not load film %1 (%2)")), _film_to_load, e.what())));
			}
		}

		if (!_film_to_create.empty ()) {
			_frame->new_film (_film_to_create);
			if (!_content_to_add.empty ()) {
				_frame->film()->examine_and_add_content (content_factory (_frame->film(), _content_to_add));
			}
		}

		ui_signaller = new wxUISignaller (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		Bind (wxEVT_TIMER, boost::bind (&App::check, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		UpdateChecker::instance()->StateChanged.connect (boost::bind (&App::update_checker_state_changed, this));
		if (Config::instance()->check_for_updates ()) {
			UpdateChecker::instance()->run ();
		}

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, wxString::Format ("DCP-o-matic could not start: %s", e.what ()));
		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser)
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser)
	{
		if (parser.GetParamCount() > 0) {
			if (parser.Found (wxT ("new"))) {
				_film_to_create = wx_to_std (parser.GetParam (0));
			} else {
				_film_to_load = wx_to_std (parser.GetParam (0));
			}
		}

		wxString content;
		if (parser.Found (wxT ("content"), &content)) {
			_content_to_add = wx_to_std (content);
		}

		return true;
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (0, wxString::Format (_("An exception occurred: %s in %s.\n\n" + REPORT_PROBLEM), e.what(), e.file().string().c_str ()));
		} catch (exception& e) {
			error_dialog (0, wxString::Format (_("An exception occurred: %s.\n\n"), e.what ()) + "  " + REPORT_PROBLEM);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}

		/* This will terminate the program */
		return false;
	}
	
	void OnUnhandledException ()
	{
		error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
	}

	void idle ()
	{
		ui_signaller->ui_idle ();
	}

	void check ()
	{
		try {
			ServerFinder::instance()->rethrow ();
		} catch (exception& e) {
			error_dialog (0, std_to_wx (e.what ()));
		}
	}

	void update_checker_state_changed ()
	{
		UpdateChecker* uc = UpdateChecker::instance ();
		if (uc->state() == UpdateChecker::YES && (uc->stable() || uc->test())) {
			UpdateDialog* dialog = new UpdateDialog (_frame, uc->stable (), uc->test ());
			dialog->ShowModal ();
			dialog->Destroy ();
		} else if (uc->state() == UpdateChecker::FAILED) {
			if (!UpdateChecker::instance()->last_emit_was_first ()) {
				error_dialog (_frame, _("The DCP-o-matic download server could not be contacted."));
			}
		} else {
			if (!UpdateChecker::instance()->last_emit_was_first ()) {
				error_dialog (_frame, _("There are no new versions of DCP-o-matic available."));
			}
		}
	}

	Frame* _frame;
	shared_ptr<wxTimer> _timer;
	string _film_to_load;
	string _film_to_create;
	string _content_to_add;
};

IMPLEMENT_APP (App)
