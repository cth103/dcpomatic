/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/tools/dcpomatic.cc
 *  @brief The main DCP-o-matic GUI.
 */

#include "wx/film_viewer.h"
#include "wx/film_editor.h"
#include "wx/job_manager_view.h"
#include "wx/full_config_dialog.h"
#include "wx/wx_util.h"
#include "wx/film_name_location_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/about_dialog.h"
#include "wx/kdm_dialog.h"
#include "wx/self_dkdm_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/hints_dialog.h"
#include "wx/update_dialog.h"
#include "wx/content_panel.h"
#include "wx/report_problem_dialog.h"
#include "wx/video_waveform_dialog.h"
#include "wx/save_template_dialog.h"
#include "wx/templates_dialog.h"
#include "wx/nag_dialog.h"
#include "wx/export_dialog.h"
#include "wx/paste_dialog.h"
#include "lib/film.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include "lib/content.h"
#include "lib/version.h"
#include "lib/signal_manager.h"
#include "lib/log.h"
#include "lib/job_manager.h"
#include "lib/exceptions.h"
#include "lib/cinema.h"
#include "lib/screen_kdm.h"
#include "lib/send_kdm_email_job.h"
#include "lib/encode_server_finder.h"
#include "lib/update_checker.h"
#include "lib/cross.h"
#include "lib/content_factory.h"
#include "lib/compose.hpp"
#include "lib/cinema_kdms.h"
#include "lib/dcpomatic_socket.h"
#include "lib/hints.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_encoder.h"
#include "lib/transcode_job.h"
#include "lib/dkdm_wrapper.h"
#include "lib/audio_content.h"
#include "lib/subtitle_content.h"
#include <dcp/exceptions.h>
#include <dcp/raw_convert.h>
#include <wx/generic/aboutdlgg.h>
#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <fstream>
/* This is OK as it's only used with DCPOMATIC_WINDOWS */
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
using boost::optional;
using dcp::raw_convert;

class FilmChangedClosingDialog : public boost::noncopyable
{
public:
	FilmChangedClosingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			0,
			wxString::Format (_("Save changes to film \"%s\" before closing?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			);

		_dialog->SetYesNoCancelLabels (
			_("Save film and close"), _("Close without saving film"), _("Don't close")
			);
	}

	~FilmChangedClosingDialog ()
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

class FilmChangedDuplicatingDialog : public boost::noncopyable
{
public:
	FilmChangedDuplicatingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			0,
			wxString::Format (_("Save changes to film \"%s\" before duplicating?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			);

		_dialog->SetYesNoCancelLabels (
			_("Save film and duplicate"), _("Duplicate without saving film"), _("Don't duplicate")
			);
	}

	~FilmChangedDuplicatingDialog ()
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

#define ALWAYS                        0x0
#define NEEDS_FILM                    0x1
#define NOT_DURING_DCP_CREATION       0x2
#define NEEDS_CPL                     0x4
#define NEEDS_SINGLE_SELECTED_CONTENT 0x8
#define NEEDS_SELECTED_CONTENT        0x10
#define NEEDS_SELECTED_VIDEO_CONTENT  0x20
#define NEEDS_CLIPBOARD               0x40
#define NEEDS_ENCRYPTION              0x80

map<wxMenuItem*, int> menu_items;

enum {
	ID_file_new = 1,
	ID_file_open,
	ID_file_save,
	ID_file_save_as_template,
	ID_file_duplicate,
	ID_file_duplicate_and_open,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_edit_copy = 100,
	ID_edit_paste,
	ID_content_scale_to_fit_width,
	ID_content_scale_to_fit_height,
	ID_jobs_make_dcp,
	ID_jobs_make_dcp_batch,
	ID_jobs_make_kdms,
	ID_jobs_make_self_dkdm,
	ID_jobs_export,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_show_dcp,
	ID_tools_video_waveform,
	ID_tools_hints,
	ID_tools_encoding_servers,
	ID_tools_manage_templates,
	ID_tools_check_for_updates,
	ID_tools_restore_default_preferences,
	ID_help_report_a_problem,
	/* IDs for shortcuts (with no associated menu item) */
	ID_add_file,
	ID_remove
};

class DOMFrame : public wxFrame
{
public:
	DOMFrame (wxString const & title)
		: wxFrame (NULL, -1, title)
		, _video_waveform_dialog (0)
		, _hints_dialog (0)
		, _servers_list_dialog (0)
		, _config_dialog (0)
		, _kdm_dialog (0)
		, _templates_dialog (0)
		, _file_menu (0)
		, _history_items (0)
		, _history_position (0)
		, _history_separator (0)
		, _update_news_requested (false)
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

#ifdef DCPOMATIC_WINDOWS
		SetIcon (wxIcon (std_to_wx ("id")));
#endif

		_config_changed_connection = Config::instance()->Changed.connect (boost::bind (&DOMFrame::config_changed, this, _1));
		config_changed (Config::OTHER);

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_new, this),                ID_file_new);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_open, this),               ID_file_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_save, this),               ID_file_save);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_save_as_template, this),   ID_file_save_as_template);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_duplicate, this),          ID_file_duplicate);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_duplicate_and_open, this), ID_file_duplicate_and_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_history, this, _1),        ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this),               wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_copy, this),               ID_edit_copy);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_paste, this),              ID_edit_paste);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this),        wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::content_scale_to_fit_width, this), ID_content_scale_to_fit_width);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::content_scale_to_fit_height, this), ID_content_scale_to_fit_height);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_dcp, this),           ID_jobs_make_dcp);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_kdms, this),          ID_jobs_make_kdms);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_dcp_batch, this),     ID_jobs_make_dcp_batch);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_self_dkdm, this),     ID_jobs_make_self_dkdm);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_export, this),             ID_jobs_export);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_send_dcp_to_tms, this),    ID_jobs_send_dcp_to_tms);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_show_dcp, this),           ID_jobs_show_dcp);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_video_waveform, this),    ID_tools_video_waveform);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_hints, this),             ID_tools_hints);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_encoding_servers, this),  ID_tools_encoding_servers);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_manage_templates, this),  ID_tools_manage_templates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_restore_default_preferences, this), ID_tools_restore_default_preferences);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),              wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this),   ID_help_report_a_problem);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&DOMFrame::close, this, _1));

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);

		_film_viewer = new FilmViewer (overall_panel);
		_film_editor = new FilmEditor (overall_panel, _film_viewer);
		JobManagerView* job_manager_view = new JobManagerView (overall_panel, false);

		wxBoxSizer* right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add (_film_viewer, 2, wxEXPAND | wxALL, 6);
		right_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL, 6);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (_film_editor, 1, wxEXPAND | wxALL, 6);
		main_sizer->Add (right_sizer, 2, wxEXPAND | wxALL, 6);

		set_menu_sensitivity ();

		_film_editor->FileChanged.connect (bind (&DOMFrame::file_changed, this, _1));
		_film_editor->content_panel()->SelectionChanged.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));
		file_changed ("");

		JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));

		overall_panel->SetSizer (main_sizer);

#ifdef __WXOSX__
		int accelerators = 3;
#else
		int accelerators = 2;
#endif
		wxAcceleratorEntry* accel = new wxAcceleratorEntry[accelerators];
		accel[0].Set (wxACCEL_CTRL, static_cast<int>('A'), ID_add_file);
		accel[1].Set (wxACCEL_NORMAL, WXK_DELETE, ID_remove);
#ifdef __WXOSX__
		accel[2].Set (wxACCEL_CTRL, static_cast<int>('W'), wxID_EXIT);
#endif
		Bind (wxEVT_MENU, boost::bind (&ContentPanel::add_file_clicked, _film_editor->content_panel()), ID_add_file);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::remove_clicked, this, _1), ID_remove);
		wxAcceleratorTable accel_table (accelerators, accel);
		SetAcceleratorTable (accel_table);
		delete[] accel;

		UpdateChecker::instance()->StateChanged.connect (boost::bind (&DOMFrame::update_checker_state_changed, this));
	}

	void remove_clicked (wxCommandEvent& ev)
	{
		if (_film_editor->content_panel()->remove_clicked (true)) {
			ev.Skip ();
		}
	}

	void new_film (boost::filesystem::path path, optional<string> template_name)
	{
		shared_ptr<Film> film (new Film (path));
		if (template_name) {
			film->use_template (template_name.get());
		}
		film->set_name (path.filename().generic_string());
		film->write_metadata ();
		set_film (film);
	}

	void load_film (boost::filesystem::path file)
	try
	{
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
		error_dialog (this, wxString::Format (_("Could not open film at %s"), p.data()), std_to_wx (e.what()));
	}

	void set_film (shared_ptr<Film> film)
	{
		_film = film;
		_film_viewer->set_film (_film);
		_film_editor->set_film (_film);
		delete _video_waveform_dialog;
		_video_waveform_dialog = 0;
		set_menu_sensitivity ();
		if (_film->directory()) {
			Config::instance()->add_to_history (_film->directory().get());
		}
		_film->Changed.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));
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
		FilmNameLocationDialog* d = new FilmNameLocationDialog (this, _("New Film"), true);
		int const r = d->ShowModal ();

		if (r == wxID_OK && d->check_path() && maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			new_film (d->path(), d->template_name());
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

		if (r == wxID_OK && maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			load_film (wx_to_std (c->GetPath ()));
		}

		c->Destroy ();
	}

	void file_save ()
	{
		_film->write_metadata ();
	}

	void file_save_as_template ()
	{
		SaveTemplateDialog* d = new SaveTemplateDialog (this);
		int const r = d->ShowModal ();
		if (r == wxID_OK) {
			Config::instance()->save_template (_film, d->name ());
		}
		d->Destroy ();
	}

	void file_duplicate ()
	{
		FilmNameLocationDialog* d = new FilmNameLocationDialog (this, _("Duplicate Film"), false);
		int const r = d->ShowModal ();

		if (r == wxID_OK && d->check_path() && maybe_save_film<FilmChangedDuplicatingDialog>()) {
			shared_ptr<Film> film (new Film (d->path()));
			film->copy_from (_film);
			film->set_name (d->path().filename().generic_string());
			film->write_metadata ();
		}

		d->Destroy ();
	}

	void file_duplicate_and_open ()
	{
		FilmNameLocationDialog* d = new FilmNameLocationDialog (this, _("Duplicate Film"), false);
		int const r = d->ShowModal ();

		if (r == wxID_OK && d->check_path() && maybe_save_film<FilmChangedDuplicatingDialog>()) {
			shared_ptr<Film> film (new Film (d->path()));
			film->copy_from (_film);
			film->set_name (d->path().filename().generic_string());
			film->write_metadata ();
			set_film (film);
		}

		d->Destroy ();
	}

	void file_history (wxCommandEvent& event)
	{
		vector<boost::filesystem::path> history = Config::instance()->history ();
		int n = event.GetId() - ID_file_history;
		if (n >= 0 && n < static_cast<int> (history.size ()) && maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			load_film (history[n]);
		}
	}

	void file_exit ()
	{
		/* false here allows the close handler to veto the close request */
		Close (false);
	}

	void edit_copy ()
	{
		ContentList const sel = _film_editor->content_panel()->selected();
		DCPOMATIC_ASSERT (sel.size() == 1);
		_clipboard = sel.front()->clone();
	}

	void edit_paste ()
	{
		DCPOMATIC_ASSERT (_clipboard);

		PasteDialog* d = new PasteDialog (this, static_cast<bool>(_clipboard->video), static_cast<bool>(_clipboard->audio), static_cast<bool>(_clipboard->subtitle));
		if (d->ShowModal() == wxID_OK) {
			BOOST_FOREACH (shared_ptr<Content> i, _film_editor->content_panel()->selected()) {
				if (d->video() && i->video) {
					DCPOMATIC_ASSERT (_clipboard->video);
					i->video->take_settings_from (_clipboard->video);
				}
				if (d->audio() && i->audio) {
					DCPOMATIC_ASSERT (_clipboard->audio);
					i->audio->take_settings_from (_clipboard->audio);
				}
				if (d->subtitle() && i->subtitle) {
					DCPOMATIC_ASSERT (_clipboard->subtitle);
					i->subtitle->take_settings_from (_clipboard->subtitle);
				}
			}
		}
		d->Destroy ();
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_full_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void tools_restore_default_preferences ()
	{
		wxMessageDialog* d = new wxMessageDialog (
			0,
			_("Are you sure you want to restore preferences to their defaults?  This cannot be undone."),
			_("Restore default preferences"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		int const r = d->ShowModal ();
		d->Destroy ();

		if (r == wxID_YES) {
			Config::restore_defaults ();
		}
	}

	void jobs_make_dcp ()
	{
		double required;
		double available;
		bool can_hard_link;

		if (!_film->should_be_enough_disk_space (required, available, can_hard_link)) {
			wxString message;
			if (can_hard_link) {
				message = wxString::Format (_("The DCP for this film will take up about %.1f Gb, and the disk that you are using only has %.1f Gb available.  Do you want to continue anyway?"), required, available);
			} else {
				message = wxString::Format (_("The DCP and intermediate files for this film will take up about %.1f Gb, and the disk that you are using only has %.1f Gb available.  You would need half as much space if the filesystem supported hard links, but it does not.  Do you want to continue anyway?"), required, available);
			}
			if (!confirm_dialog (this, message)) {
				return;
			}
		}

		if (!get_hints(_film).empty() && Config::instance()->show_hints_before_make_dcp()) {
			HintsDialog* hints = new HintsDialog (this, _film, false);
			int const r = hints->ShowModal();
			hints->Destroy ();
			if (r == wxID_CANCEL) {
				return;
			}
		}

		if (_film->encrypted ()) {
			NagDialog::maybe_nag (
				this,
				Config::NAG_ENCRYPTED_METADATA,
				_("You are making an encrypted DCP.  It will not be possible to make KDMs for this DCP unless you have copies of "
				  "the <tt>metadata.xml</tt> file within the film and the metadata files within the DCP.\n\n"
				  "You should ensure that these files are <span weight=\"bold\" size=\"larger\">BACKED UP</span> "
				  "if you want to make KDMs for this film.")
				);
		}

		/* Remove any existing DCP if the user agrees */
		boost::filesystem::path const dcp_dir = _film->dir (_film->dcp_name(), false);
		if (boost::filesystem::exists (dcp_dir)) {
			if (!confirm_dialog (this, wxString::Format (_("Do you want to overwrite the existing DCP %s?"), std_to_wx(dcp_dir.string()).data()))) {
				return;
			}
			boost::filesystem::remove_all (dcp_dir);
		}

		try {
			/* It seems to make sense to auto-save metadata here, since the make DCP may last
			   a long time, and crashes/power failures are moderately likely.
			*/
			_film->write_metadata ();
			_film->make_dcp ();
		} catch (BadSettingError& e) {
			error_dialog (this, wxString::Format (_("Bad setting for %s."), std_to_wx(e.setting()).data()), std_to_wx(e.what()));
		} catch (std::exception& e) {
			error_dialog (this, wxString::Format (_("Could not make DCP.")), std_to_wx(e.what()));
		}
	}

	void jobs_make_kdms ()
	{
		if (!_film) {
			return;
		}

		if (_kdm_dialog) {
			_kdm_dialog->Destroy ();
			_kdm_dialog = 0;
		}

		_kdm_dialog = new KDMDialog (this, _film);
		_kdm_dialog->Show ();
	}

	void jobs_make_dcp_batch ()
	{
		if (!_film) {
			return;
		}

		if (!get_hints(_film).empty() && Config::instance()->show_hints_before_make_dcp()) {
			HintsDialog* hints = new HintsDialog (this, _film, false);
			int const r = hints->ShowModal();
			hints->Destroy ();
			if (r == wxID_CANCEL) {
				return;
			}
		}

		_film->write_metadata ();

		/* i = 0; try to connect via socket
		   i = 1; try again, and then try to start the batch converter
		   i = 2 onwards; try again.
		*/
		for (int i = 0; i < 8; ++i) {
			try {
				boost::asio::io_service io_service;
				boost::asio::ip::tcp::resolver resolver (io_service);
				boost::asio::ip::tcp::resolver::query query ("127.0.0.1", raw_convert<string> (BATCH_JOB_PORT));
				boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);
				Socket socket (5);
				socket.connect (*endpoint_iterator);
				DCPOMATIC_ASSERT (_film->directory ());
				string s = _film->directory()->string ();
				socket.write (s.length() + 1);
				socket.write ((uint8_t *) s.c_str(), s.length() + 1);
				/* OK\0 */
				uint8_t ok[3];
				socket.read (ok, 3);
				return;
			} catch (exception& e) {

			}

			if (i == 1) {
				start_batch_converter (wx_to_std (wxStandardPaths::Get().GetExecutablePath()));
			}

			dcpomatic_sleep (1);
		}

		error_dialog (this, _("Could not find batch converter."));
	}

	void jobs_make_self_dkdm ()
	{
		if (!_film) {
			return;
		}

		SelfDKDMDialog* d = new SelfDKDMDialog (this, _film);
		if (d->ShowModal () != wxID_OK) {
			d->Destroy ();
			return;
		}

		NagDialog::maybe_nag (
			this,
			Config::NAG_DKDM_CONFIG,
			wxString::Format (
				_("You are making a DKDM which is encrypted by a private key held in"
				  "\n\n<tt>%s</tt>\n\nIt is <span weight=\"bold\" size=\"larger\">VITALLY IMPORTANT</span> "
				  "that you <span weight=\"bold\" size=\"larger\">BACK UP THIS FILE</span> since if it is lost "
				  "your DKDMs (and the DCPs they protect) will become useless."), std_to_wx(Config::config_file().string()).data()
				)
			);

		optional<dcp::EncryptedKDM> kdm;
		try {
			kdm = _film->make_kdm (
				Config::instance()->decryption_chain()->leaf(),
				vector<dcp::Certificate> (),
				d->cpl (),
				dcp::LocalTime ("2012-01-01T01:00:00+00:00"),
				dcp::LocalTime ("2112-01-01T01:00:00+00:00"),
				dcp::MODIFIED_TRANSITIONAL_1,
				-1,
				-1
				);
		} catch (dcp::NotEncryptedError& e) {
			error_dialog (this, _("CPL's content is not encrypted."));
		} catch (exception& e) {
			error_dialog (this, e.what ());
		} catch (...) {
			error_dialog (this, _("An unknown exception occurred."));
		}

		if (kdm) {
			if (d->internal ()) {
				shared_ptr<DKDMGroup> dkdms = Config::instance()->dkdms ();
				dkdms->add (shared_ptr<DKDM> (new DKDM (kdm.get())));
				Config::instance()->changed ();
			} else {
				boost::filesystem::path path = d->directory() / (_film->dcp_name(false) + "_DKDM.xml");
				kdm->as_xml (path);
			}
		}

		d->Destroy ();
	}

	void jobs_export ()
	{
		ExportDialog* d = new ExportDialog (this);
		if (d->ShowModal() == wxID_OK) {
			shared_ptr<TranscodeJob> job (new TranscodeJob (_film));
			job->set_encoder (shared_ptr<FFmpegEncoder> (new FFmpegEncoder (_film, job, d->path(), d->format(), d->mixdown_to_stereo())));
			JobManager::instance()->add (job);
		}
		d->Destroy ();
	}

	void content_scale_to_fit_width ()
	{
		ContentList vc = _film_editor->content_panel()->selected_video ();
		for (ContentList::iterator i = vc.begin(); i != vc.end(); ++i) {
			(*i)->video->scale_and_crop_to_fit_width ();
		}
	}

	void content_scale_to_fit_height ()
	{
		ContentList vc = _film_editor->content_panel()->selected_video ();
		for (ContentList::iterator i = vc.begin(); i != vc.end(); ++i) {
			(*i)->video->scale_and_crop_to_fit_height ();
		}
	}

	void jobs_send_dcp_to_tms ()
	{
		_film->send_dcp_to_tms ();
	}

	void jobs_show_dcp ()
	{
		DCPOMATIC_ASSERT (_film->directory ());
#ifdef DCPOMATIC_WINDOWS
		wstringstream args;
		args << "/select," << _film->dir (_film->dcp_name(false));
		ShellExecute (0, L"open", L"explorer.exe", args.str().c_str(), 0, SW_SHOWDEFAULT);
#endif

#ifdef DCPOMATIC_LINUX
		int r = system ("which nautilus");
		if (WEXITSTATUS (r) == 0) {
			r = system (String::compose("nautilus \"%1\"", _film->directory()->string()).c_str());
			if (WEXITSTATUS (r)) {
				error_dialog (this, _("Could not show DCP."), _("Could not run nautilus"));
			}
		} else {
			int r = system ("which konqueror");
			if (WEXITSTATUS (r) == 0) {
				r = system (String::compose ("konqueror \"%1\"", _film->directory()->string()).c_str());
				if (WEXITSTATUS (r)) {
					error_dialog (this, _("Could not show DCP"), _("Could not run konqueror"));
				}
			}
		}
#endif

#ifdef DCPOMATIC_OSX
		int r = system (String::compose ("open -R \"%1\"", _film->dir (_film->dcp_name(false)).string()).c_str());
		if (WEXITSTATUS (r)) {
			error_dialog (this, _("Could not show DCP"));
		}
#endif
	}

	void tools_video_waveform ()
	{
		if (!_video_waveform_dialog) {
			_video_waveform_dialog = new VideoWaveformDialog (this, _film, _film_viewer);
		}

		_video_waveform_dialog->Show ();
	}

	void tools_hints ()
	{
		if (!_hints_dialog) {
			_hints_dialog = new HintsDialog (this, _film, true);
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

	void tools_manage_templates ()
	{
		if (!_templates_dialog) {
			_templates_dialog = new TemplatesDialog (this);
		}

		_templates_dialog->Show ();
	}

	void tools_check_for_updates ()
	{
		UpdateChecker::instance()->run ();
		_update_news_requested = true;
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

		if (_film && _film->dirty ()) {

			FilmChangedClosingDialog* dialog = new FilmChangedClosingDialog (_film->name ());
			int const r = dialog->run ();
			delete dialog;

			switch (r) {
			case wxID_NO:
				/* Don't save and carry on to close */
				break;
			case wxID_YES:
				/* Save and carry on to close */
				_film->write_metadata ();
				break;
			case wxID_CANCEL:
				/* Veto the event and stop */
				ev.Veto ();
				return;
			}
		}

		/* We don't want to hear about any more configuration changes, since they
		   cause the File menu to be altered, which itself will be deleted around
		   now (without, as far as I can see, any way for us to find out).
		*/
		_config_changed_connection.disconnect ();

		ev.Skip ();
	}

	void set_menu_sensitivity ()
	{
		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();
		list<shared_ptr<Job> >::iterator i = jobs.begin();
		while (i != jobs.end() && (*i)->json_name() != "transcode") {
			++i;
		}
		bool const dcp_creation = (i != jobs.end ()) && !(*i)->finished ();
		bool const have_cpl = _film && !_film->cpls().empty ();
		bool const have_single_selected_content = _film_editor->content_panel()->selected().size() == 1;
		bool const have_selected_content = !_film_editor->content_panel()->selected().empty();
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

			if ((j->second & NEEDS_SELECTED_CONTENT) && !have_selected_content) {
				enabled = false;
			}

			if ((j->second & NEEDS_SINGLE_SELECTED_CONTENT) && !have_single_selected_content) {
				enabled = false;
			}

			if ((j->second & NEEDS_SELECTED_VIDEO_CONTENT) && !have_selected_video_content) {
				enabled = false;
			}

			if ((j->second & NEEDS_CLIPBOARD) && !_clipboard) {
				enabled = false;
			}

			if ((j->second & NEEDS_ENCRYPTION) && (!_film || !_film->encrypted())) {
				enabled = false;
			}

			j->first->Enable (enabled);
		}
	}

	/** @return true if the operation that called this method
	 *  should continue, false to abort it.
	 */
	template <class T>
	bool maybe_save_film ()
	{
		if (!_film) {
			return true;
		}

		if (_film->dirty ()) {
			T d (_film->name ());
			switch (d.run ()) {
			case wxID_NO:
				return true;
			case wxID_YES:
				_film->write_metadata ();
				return true;
			case wxID_CANCEL:
				return false;
			}
		}

		return true;
	}

	template <class T>
	bool maybe_save_then_delete_film ()
	{
		bool const r = maybe_save_film<T> ();
		if (r) {
			_film.reset ();
		}
		return r;
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
		add_item (_file_menu, _("Save as &template..."), ID_file_save_as_template, NEEDS_FILM);
		add_item (_file_menu, _("Duplicate..."), ID_file_duplicate, NEEDS_FILM);
		add_item (_file_menu, _("Duplicate and open..."), ID_file_duplicate_and_open, NEEDS_FILM);

		_history_position = _file_menu->GetMenuItems().GetCount();

#ifndef __WXOSX__
		_file_menu->AppendSeparator ();
#endif

#ifdef __WXOSX__
		add_item (_file_menu, _("&Exit"), wxID_EXIT, ALWAYS);
#else
		add_item (_file_menu, _("&Quit"), wxID_EXIT, ALWAYS);
#endif

		wxMenu* edit = new wxMenu;
		add_item (edit, _("Copy settings\tCtrl-C"), ID_edit_copy, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SINGLE_SELECTED_CONTENT);
		add_item (edit, _("Paste settings...\tCtrl-V"), ID_edit_paste, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SELECTED_CONTENT | NEEDS_CLIPBOARD);

#ifdef __WXOSX__
		add_item (_file_menu, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#else
		add_item (edit, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#endif

		wxMenu* content = new wxMenu;
		add_item (content, _("Scale to fit &width"), ID_content_scale_to_fit_width, NEEDS_FILM | NEEDS_SELECTED_VIDEO_CONTENT);
		add_item (content, _("Scale to fit &height"), ID_content_scale_to_fit_height, NEEDS_FILM | NEEDS_SELECTED_VIDEO_CONTENT);

		wxMenu* jobs_menu = new wxMenu;
		add_item (jobs_menu, _("&Make DCP\tCtrl-M"), ID_jobs_make_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		add_item (jobs_menu, _("Make DCP in &batch converter\tCtrl-B"), ID_jobs_make_dcp_batch, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("Make &KDMs...\tCtrl-K"), ID_jobs_make_kdms, NEEDS_FILM);
		add_item (jobs_menu, _("Make DKDM for DCP-o-matic..."), ID_jobs_make_self_dkdm, NEEDS_FILM | NEEDS_ENCRYPTION);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("Export...\tCtrl-E"), ID_jobs_export, NEEDS_FILM);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);
		add_item (jobs_menu, _("S&how DCP"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);

		wxMenu* tools = new wxMenu;
		add_item (tools, _("Video waveform..."), ID_tools_video_waveform, NEEDS_FILM);
		add_item (tools, _("Hints..."), ID_tools_hints, 0);
		add_item (tools, _("Encoding servers..."), ID_tools_encoding_servers, 0);
		add_item (tools, _("Manage templates..."), ID_tools_manage_templates, 0);
		add_item (tools, _("Check for updates"), ID_tools_check_for_updates, 0);
		tools->AppendSeparator ();
		add_item (tools, _("Restore default preferences"), ID_tools_restore_default_preferences, ALWAYS);

		wxMenu* help = new wxMenu;
#ifdef __WXOSX__
		add_item (help, _("About DCP-o-matic"), wxID_ABOUT, ALWAYS);
#else
		add_item (help, _("About"), wxID_ABOUT, ALWAYS);
#endif
		add_item (help, _("Report a problem..."), ID_help_report_a_problem, NEEDS_FILM);

		m->Append (_file_menu, _("&File"));
		m->Append (edit, _("&Edit"));
		m->Append (content, _("&Content"));
		m->Append (jobs_menu, _("&Jobs"));
		m->Append (tools, _("&Tools"));
		m->Append (help, _("&Help"));
	}

	void config_changed (Config::Property what)
	{
		/* Instantly save any config changes when using the DCP-o-matic GUI */
		if (what == Config::CINEMAS) {
			try {
				Config::instance()->write_cinemas();
			} catch (exception& e) {
				error_dialog (
					this,
					wxString::Format (
						_("Could not write to cinemas file at %s.  Your changes have not been saved."),
						std_to_wx (Config::instance()->cinemas_file().string()).data()
						)
					);
			}
		} else {
			try {
				Config::instance()->write_config();
			} catch (exception& e) {
				error_dialog (
					this,
					wxString::Format (
						_("Could not write to config file at %s.  Your changes have not been saved."),
						std_to_wx (Config::instance()->cinemas_file().string()).data()
						)
					);
			}
		}

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
			string s;
			if (i < 9) {
				s = String::compose ("&%1 %2", i + 1, history[i].string());
			} else {
				s = history[i].string();
			}
			_file_menu->Insert (pos++, ID_file_history + i, std_to_wx (s));
		}

		_history_items = history.size ();
	}

	void update_checker_state_changed ()
	{
		UpdateChecker* uc = UpdateChecker::instance ();

		bool const announce =
			_update_news_requested ||
			(uc->stable() && Config::instance()->check_for_updates()) ||
			(uc->test() && Config::instance()->check_for_updates() && Config::instance()->check_for_test_updates());

		_update_news_requested = false;

		if (!announce) {
			return;
		}

		if (uc->state() == UpdateChecker::YES) {
			UpdateDialog* dialog = new UpdateDialog (this, uc->stable (), uc->test ());
			dialog->ShowModal ();
			dialog->Destroy ();
		} else if (uc->state() == UpdateChecker::FAILED) {
			error_dialog (this, _("The DCP-o-matic download server could not be contacted."));
		} else {
			error_dialog (this, _("There are no new versions of DCP-o-matic available."));
		}

		_update_news_requested = false;
	}

	FilmEditor* _film_editor;
	FilmViewer* _film_viewer;
	VideoWaveformDialog* _video_waveform_dialog;
	HintsDialog* _hints_dialog;
	ServersListDialog* _servers_list_dialog;
	wxPreferencesEditor* _config_dialog;
	KDMDialog* _kdm_dialog;
	TemplatesDialog* _templates_dialog;
	wxMenu* _file_menu;
	shared_ptr<Film> _film;
	int _history_items;
	int _history_position;
	wxMenuItem* _history_separator;
	boost::signals2::scoped_connection _config_changed_connection;
	bool _update_news_requested;
	shared_ptr<Content> _clipboard;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_SWITCH, "n", "new", "create new film", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "content", "add content file / directory", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "d", "dcp", "add content DCP", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
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
		wxInitAllImageHandlers ();

		Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
		Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

		wxSplashScreen* splash = maybe_show_splash ();

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

		dcpomatic_setup_path_encoding ();

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

		_frame = new DOMFrame (_("DCP-o-matic"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		if (splash) {
			splash->Destroy ();
		}
		_frame->Show ();

		if (!_film_to_load.empty() && boost::filesystem::is_directory (_film_to_load)) {
			try {
				_frame->load_film (_film_to_load);
			} catch (exception& e) {
				error_dialog (0, std_to_wx (String::compose (wx_to_std (_("Could not load film %1 (%2)")), _film_to_load)), std_to_wx(e.what()));
			}
		}

		if (!_film_to_create.empty ()) {
			_frame->new_film (_film_to_create, optional<string> ());
			if (!_content_to_add.empty ()) {
				BOOST_FOREACH (shared_ptr<Content> i, content_factory (_frame->film(), _content_to_add)) {
					_frame->film()->examine_and_add_content (i);
				}
			}
			if (!_dcp_to_add.empty ()) {
				_frame->film()->examine_and_add_content (shared_ptr<DCPContent> (new DCPContent (_frame->film(), _dcp_to_add)));
			}
		}

		signal_manager = new wxSignalManager (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		Bind (wxEVT_TIMER, boost::bind (&App::check, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		if (Config::instance()->check_for_updates ()) {
			UpdateChecker::instance()->run ();
		}

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, wxString::Format ("DCP-o-matic could not start."), std_to_wx(e.what()));
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

		wxString dcp;
		if (parser.Found (wxT ("dcp"), &dcp)) {
			_dcp_to_add = wx_to_std (dcp);
		}

		return true;
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str ())
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop ()
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException ()
	{
		report_exception ();
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void check ()
	{
		try {
			EncodeServerFinder::instance()->rethrow ();
		} catch (exception& e) {
			error_dialog (0, std_to_wx (e.what ()));
		}
	}

	void config_failed_to_load ()
	{
		message_dialog (_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx (m));
	}

	DOMFrame* _frame;
	shared_ptr<wxTimer> _timer;
	string _film_to_load;
	string _film_to_create;
	string _content_to_add;
	string _dcp_to_add;
};

IMPLEMENT_APP (App)
