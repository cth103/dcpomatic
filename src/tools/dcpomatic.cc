/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/standard_controls.h"
#include "wx/film_viewer.h"
#include "wx/film_editor.h"
#include "wx/job_manager_view.h"
#include "wx/full_config_dialog.h"
#include "wx/wx_util.h"
#include "wx/film_name_location_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/recreate_chain_dialog.h"
#include "wx/about_dialog.h"
#include "wx/kdm_dialog.h"
#include "wx/dkdm_dialog.h"
#include "wx/self_dkdm_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/hints_dialog.h"
#include "wx/update_dialog.h"
#include "wx/content_panel.h"
#include "wx/report_problem_dialog.h"
#include "wx/video_waveform_dialog.h"
#include "wx/system_information_dialog.h"
#include "wx/save_template_dialog.h"
#include "wx/templates_dialog.h"
#include "wx/nag_dialog.h"
#include "wx/export_subtitles_dialog.h"
#include "wx/export_video_file_dialog.h"
#include "wx/paste_dialog.h"
#include "wx/focus_manager.h"
#include "wx/html_dialog.h"
#include "wx/send_i18n_dialog.h"
#include "wx/i18n_hook.h"
#include "lib/film.h"
#include "lib/analytics.h"
#include "lib/emailer.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include "lib/content.h"
#include "lib/version.h"
#include "lib/signal_manager.h"
#include "lib/log.h"
#include "lib/screen.h"
#include "lib/job_manager.h"
#include "lib/exceptions.h"
#include "lib/cinema.h"
#include "lib/kdm_with_metadata.h"
#include "lib/send_kdm_email_job.h"
#include "lib/encode_server_finder.h"
#include "lib/update_checker.h"
#include "lib/cross.h"
#include "lib/content_factory.h"
#include "lib/compose.hpp"
#include "lib/dcpomatic_socket.h"
#include "lib/hints.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_encoder.h"
#include "lib/transcode_job.h"
#include "lib/dkdm_wrapper.h"
#include "lib/audio_content.h"
#include "lib/check_content_change_job.h"
#include "lib/text_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/subtitle_encoder.h"
#include "lib/warnings.h"
#include <dcp/exceptions.h>
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/generic/aboutdlgg.h>
#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#include <wx/wxhtml.h>
DCPOMATIC_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
/* This is OK as it's only used with DCPOMATIC_WINDOWS */
#include <sstream>

#ifdef check
#undef check
#endif


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;
using std::wcout;
using std::wstring;
using std::wstringstream;
using boost::optional;
using boost::is_any_of;
using boost::algorithm::find;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;


class FilmChangedClosingDialog
{
public:
	explicit FilmChangedClosingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before closing?"), std_to_wx (name).data()),
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

	FilmChangedClosingDialog (FilmChangedClosingDialog const&) = delete;
	FilmChangedClosingDialog& operator= (FilmChangedClosingDialog const&) = delete;

	int run ()
	{
		return _dialog->ShowModal ();
	}

private:
	wxMessageDialog* _dialog;
};


class FilmChangedDuplicatingDialog
{
public:
	explicit FilmChangedDuplicatingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before duplicating?"), std_to_wx (name).data()),
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

	FilmChangedDuplicatingDialog (FilmChangedDuplicatingDialog const&) = delete;
	FilmChangedDuplicatingDialog& operator= (FilmChangedDuplicatingDialog const&) = delete;

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
	ID_file_close = 100,
	ID_edit_copy,
	ID_edit_paste,
	ID_edit_select_all,
	ID_jobs_make_dcp,
	ID_jobs_make_dcp_batch,
	ID_jobs_make_kdms,
	ID_jobs_make_dkdms,
	ID_jobs_make_self_dkdm,
	ID_jobs_export_video_file,
	ID_jobs_export_subtitles,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_show_dcp,
	ID_jobs_open_dcp_in_player,
	ID_view_closed_captions,
	ID_view_video_waveform,
	ID_tools_hints,
	ID_tools_encoding_servers,
	ID_tools_manage_templates,
	ID_tools_check_for_updates,
	ID_tools_send_translations,
	ID_tools_system_information,
	ID_tools_restore_default_preferences,
	ID_help_report_a_problem,
	/* IDs for shortcuts (with no associated menu item) */
	ID_add_file,
	ID_remove,
	ID_start_stop,
	ID_timeline,
	ID_back_frame,
	ID_forward_frame
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const& title)
		: wxFrame (nullptr, -1, title)
	{
#if defined(DCPOMATIC_WINDOWS)
		if (Config::instance()->win32_console()) {
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

		auto bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

#ifdef DCPOMATIC_WINDOWS
		SetIcon (wxIcon (std_to_wx ("id")));
#endif

		_config_changed_connection = Config::instance()->Changed.connect(boost::bind(&DOMFrame::config_changed, this, _1));
		config_changed (Config::OTHER);

		_analytics_message_connection = Analytics::instance()->Message.connect(boost::bind(&DOMFrame::analytics_message, this, _1, _2));

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_new, this),                ID_file_new);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_open, this),               ID_file_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_save, this),               ID_file_save);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_save_as_template, this),   ID_file_save_as_template);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_duplicate, this),          ID_file_duplicate);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_duplicate_and_open, this), ID_file_duplicate_and_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_close, this),              ID_file_close);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_history, this, _1),        ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this),               wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_copy, this),               ID_edit_copy);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_paste, this),              ID_edit_paste);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_select_all, this),         ID_edit_select_all);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this),        wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_dcp, this),           ID_jobs_make_dcp);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_kdms, this),          ID_jobs_make_kdms);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_dkdms, this),         ID_jobs_make_dkdms);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_dcp_batch, this),     ID_jobs_make_dcp_batch);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_make_self_dkdm, this),     ID_jobs_make_self_dkdm);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_export_video_file, this),  ID_jobs_export_video_file);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_export_subtitles, this),   ID_jobs_export_subtitles);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_send_dcp_to_tms, this),    ID_jobs_send_dcp_to_tms);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_show_dcp, this),           ID_jobs_show_dcp);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::jobs_open_dcp_in_player, this), ID_jobs_open_dcp_in_player);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_closed_captions, this),    ID_view_closed_captions);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_video_waveform, this),     ID_view_video_waveform);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_hints, this),             ID_tools_hints);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_encoding_servers, this),  ID_tools_encoding_servers);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_manage_templates, this),  ID_tools_manage_templates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_send_translations, this), ID_tools_send_translations);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_system_information, this),ID_tools_system_information);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_restore_default_preferences, this), ID_tools_restore_default_preferences);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),              wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this),   ID_help_report_a_problem);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&DOMFrame::close, this, _1));
		Bind (wxEVT_SHOW, boost::bind (&DOMFrame::show, this, _1));

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this, wxID_ANY);

		_film_viewer.reset (new FilmViewer (overall_panel));
		_controls = new StandardControls (overall_panel, _film_viewer, true);
		_film_editor = new FilmEditor (overall_panel, _film_viewer);
		auto job_manager_view = new JobManagerView (overall_panel, false);

		auto right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add (_film_viewer->panel(), 2, wxEXPAND | wxALL, 6);
		right_sizer->Add (_controls, 0, wxEXPAND | wxALL, 6);
		right_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL, 6);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (_film_editor, 0, wxEXPAND | wxALL, 6);
		main_sizer->Add (right_sizer, 1, wxEXPAND | wxALL, 6);

		set_menu_sensitivity ();

		_film_editor->FileChanged.connect (bind (&DOMFrame::file_changed, this, _1));
		_film_editor->content_panel()->SelectionChanged.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));
		file_changed ("");

		JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));

		overall_panel->SetSizer (main_sizer);

		UpdateChecker::instance()->StateChanged.connect(boost::bind(&DOMFrame::update_checker_state_changed, this));

		FocusManager::instance()->SetFocus.connect (boost::bind (&DOMFrame::remove_accelerators, this));
		FocusManager::instance()->KillFocus.connect (boost::bind (&DOMFrame::add_accelerators, this));
		add_accelerators ();
	}

	void add_accelerators ()
	{
#ifdef __WXOSX__
		int accelerators = 7;
#else
		int accelerators = 6;
#endif
		wxAcceleratorEntry* accel = new wxAcceleratorEntry[accelerators];
		accel[0].Set (wxACCEL_CTRL, static_cast<int>('A'), ID_add_file);
		accel[1].Set (wxACCEL_NORMAL, WXK_DELETE, ID_remove);
		accel[2].Set (wxACCEL_NORMAL, WXK_SPACE, ID_start_stop);
		accel[3].Set (wxACCEL_CTRL, static_cast<int>('T'), ID_timeline);
		accel[4].Set (wxACCEL_NORMAL, WXK_LEFT, ID_back_frame);
		accel[5].Set (wxACCEL_NORMAL, WXK_RIGHT, ID_forward_frame);
#ifdef __WXOSX__
		accel[6].Set (wxACCEL_CTRL, static_cast<int>('W'), ID_file_close);
#endif
		Bind (wxEVT_MENU, boost::bind (&ContentPanel::add_file_clicked, _film_editor->content_panel()), ID_add_file);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::remove_clicked, this, _1), ID_remove);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::start_stop_pressed, this), ID_start_stop);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::timeline_pressed, this), ID_timeline);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::back_frame, this), ID_back_frame);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::forward_frame, this), ID_forward_frame);
		wxAcceleratorTable accel_table (accelerators, accel);
		SetAcceleratorTable (accel_table);
		delete[] accel;
	}

	void remove_accelerators ()
	{
		SetAcceleratorTable (wxAcceleratorTable ());
	}

	void remove_clicked (wxCommandEvent& ev)
	{
		if (_film_editor->content_panel()->remove_clicked (true)) {
			ev.Skip ();
		}
	}

	void new_film (boost::filesystem::path path, optional<string> template_name)
	{
		auto film = make_shared<Film>(path);
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
		auto film = make_shared<Film>(file);
		auto const notes = film->read_metadata ();

		if (film->state_version() == 4) {
			error_dialog (
				0,
				_("This film was created with an old version of DVD-o-matic and may not load correctly "
				  "in this version.  Please check the film's settings carefully.")
				);
		}

		for (auto i: notes) {
			error_dialog (0, std_to_wx(i));
		}

		set_film (film);

		JobManager::instance()->add(shared_ptr<Job>(new CheckContentChangeJob(film)));
	}
	catch (FileNotFoundError& e) {
		auto const dir = e.file().parent_path();
		if (boost::filesystem::exists(dir / "ASSETMAP") || boost::filesystem::exists(dir / "ASSETMAP.xml")) {
			error_dialog (
				this, _("Could not open this folder as a DCP-o-matic project."),
				_("It looks like you are trying to open a DCP.  File -> Open is for loading DCP-o-matic projects, not DCPs.  To import a DCP, create a new project with File -> New and then click the \"Add DCP...\" button.")
				);
		} else {
			auto const p = std_to_wx(file.string ());
			error_dialog (this, wxString::Format(_("Could not open film at %s"), p.data()), std_to_wx(e.what()));
		}

	} catch (std::exception& e) {
		auto const p = std_to_wx (file.string());
		error_dialog (this, wxString::Format(_("Could not open film at %s"), p.data()), std_to_wx(e.what()));
	}

	void set_film (shared_ptr<Film> film)
	{
		_film = film;
		_film_viewer->set_film (_film);
		_film_editor->set_film (_film);
		_controls->set_film (_film);
		if (_video_waveform_dialog) {
			_video_waveform_dialog->Destroy ();
			_video_waveform_dialog = nullptr;
		}
		set_menu_sensitivity ();
		if (_film && _film->directory()) {
			Config::instance()->add_to_history (_film->directory().get());
		}
		if (_film) {
			_film->Change.connect (boost::bind (&DOMFrame::film_change, this, _1));
			_film->Message.connect (boost::bind(&DOMFrame::film_message, this, _1));
			dcpomatic_log = _film->log ();
		}
	}

	shared_ptr<Film> film () const {
		return _film;
	}

private:

	void show (wxShowEvent& ev)
	{
		if (ev.IsShown() && !_first_shown_called) {
			_film_editor->first_shown ();
			_first_shown_called = true;
		}
	}

	void film_message (string m)
	{
		message_dialog (this, std_to_wx(m));
	}

	void film_change (ChangeType type)
	{
		if (type == ChangeType::DONE) {
			set_menu_sensitivity ();
		}
	}

	void file_changed (boost::filesystem::path f)
	{
		auto s = wx_to_std(_("DCP-o-matic"));
		if (!f.empty ()) {
			s += " - " + f.string ();
		}

		SetTitle (std_to_wx (s));
	}

	void file_new ()
	{
		auto d = new FilmNameLocationDialog (this, _("New Film"), true);
		int const r = d->ShowModal ();

		if (r == wxID_OK && d->check_path() && maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			try {
				new_film (d->path(), d->template_name());
			} catch (boost::filesystem::filesystem_error& e) {
#ifdef DCPOMATIC_WINDOWS
				string bad_chars = "<>:\"/|?*";
				string const filename = d->path().filename().string();
				string found_bad_chars;
				for (size_t i = 0; i < bad_chars.length(); ++i) {
					if (filename.find(bad_chars[i]) != string::npos && found_bad_chars.find(bad_chars[i]) == string::npos) {
						found_bad_chars += bad_chars[i];
					}
				}
				wxString message = _("Could not create folder to store film.");
				message += "  ";
				if (!found_bad_chars.empty()) {
					message += wxString::Format (_("Try removing the %s characters from your folder name."), std_to_wx(found_bad_chars).data());
				} else {
					message += _("Please check that you do not have Windows controlled folder access enabled for DCP-o-matic.");
				}
				error_dialog (this, message, std_to_wx(e.what()));
#else
				error_dialog (this, _("Could not create folder to store film."), std_to_wx(e.what()));
#endif
			}
		}

		d->Destroy ();
	}

	void file_open ()
	{
		auto c = new wxDirDialog (
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
		auto d = new SaveTemplateDialog (this);
		int const r = d->ShowModal ();
		if (r == wxID_OK) {
			Config::instance()->save_template (_film, d->name ());
		}
		d->Destroy ();
	}

	void file_duplicate ()
	{
		auto d = new FilmNameLocationDialog (this, _("Duplicate Film"), false);
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
		auto d = new FilmNameLocationDialog (this, _("Duplicate Film"), false);
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

	void file_close ()
	{
		if (_film && _film->dirty ()) {

			auto dialog = new FilmChangedClosingDialog (_film->name ());
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
				/* Stop */
				return;
			}
		}

		set_film (shared_ptr<Film>());
	}

	void file_history (wxCommandEvent& event)
	{
		auto history = Config::instance()->history ();
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
		auto const sel = _film_editor->content_panel()->selected();
		DCPOMATIC_ASSERT (sel.size() == 1);
		_clipboard = sel.front()->clone();
	}

	void edit_paste ()
	{
		DCPOMATIC_ASSERT (_clipboard);

		auto d = new PasteDialog (this, static_cast<bool>(_clipboard->video), static_cast<bool>(_clipboard->audio), !_clipboard->text.empty());
		if (d->ShowModal() == wxID_OK) {
			for (auto i: _film_editor->content_panel()->selected()) {
				if (d->video() && i->video) {
					DCPOMATIC_ASSERT (_clipboard->video);
					i->video->take_settings_from (_clipboard->video);
				}
				if (d->audio() && i->audio) {
					DCPOMATIC_ASSERT (_clipboard->audio);
					i->audio->take_settings_from (_clipboard->audio);
				}

				if (d->text()) {
					auto j = i->text.begin ();
					auto k = _clipboard->text.begin ();
					while (j != i->text.end() && k != _clipboard->text.end()) {
						(*j)->take_settings_from (*k);
						++j;
						++k;
					}
				}
			}
		}
		d->Destroy ();
	}

	void edit_select_all ()
	{
		_film_editor->content_panel()->select_all();
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
		auto d = new wxMessageDialog (
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
				message = wxString::Format (_("The DCP for this film will take up about %.1f GB, and the disk that you are using only has %.1f GB available.  Do you want to continue anyway?"), required, available);
			} else {
				message = wxString::Format (_("The DCP and intermediate files for this film will take up about %.1f GB, and the disk that you are using only has %.1f GB available.  You would need half as much space if the filesystem supported hard links, but it does not.  Do you want to continue anyway?"), required, available);
			}
			if (!confirm_dialog (this, message)) {
				return;
			}
		}

		if (Config::instance()->show_hints_before_make_dcp()) {
			auto hints = new HintsDialog (this, _film, false);
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
		auto const dcp_dir = _film->dir (_film->dcp_name(), false);
		if (boost::filesystem::exists(dcp_dir)) {
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
			_film->make_dcp (true);
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

	void jobs_make_dkdms ()
	{
		if (!_film) {
			return;
		}

		if (_dkdm_dialog) {
			_dkdm_dialog->Destroy ();
			_dkdm_dialog = nullptr;
		}

		_dkdm_dialog = new DKDMDialog (this, _film);
		_dkdm_dialog->Show ();
	}

	/** @return false if we succeeded, true if not */
	bool send_to_other_tool (int port, function<void()> start, string message)
	{
		/* i = 0; try to connect via socket
		   i = 1; try again, and then try to start the tool
		   i = 2 onwards; try again.
		*/
		for (int i = 0; i < 8; ++i) {
			try {
				boost::asio::io_service io_service;
				boost::asio::ip::tcp::resolver resolver (io_service);
				boost::asio::ip::tcp::resolver::query query ("127.0.0.1", raw_convert<string> (port));
				boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);
				Socket socket (5);
				socket.connect (*endpoint_iterator);
				DCPOMATIC_ASSERT (_film->directory ());
				socket.write (message.length() + 1);
				socket.write ((uint8_t *) message.c_str(), message.length() + 1);
				/* OK\0 */
				uint8_t ok[3];
				socket.read (ok, 3);
				return false;
			} catch (exception& e) {

			}

			if (i == 1) {
				start ();
			}

			dcpomatic_sleep_seconds (1);
		}

		return true;
	}

	void jobs_make_dcp_batch ()
	{
		if (!_film) {
			return;
		}

		if (Config::instance()->show_hints_before_make_dcp()) {
			auto hints = new HintsDialog (this, _film, false);
			int const r = hints->ShowModal();
			hints->Destroy ();
			if (r == wxID_CANCEL) {
				return;
			}
		}

		_film->write_metadata ();

		if (send_to_other_tool (BATCH_JOB_PORT, &start_batch_converter, _film->directory()->string())) {
#ifdef DCPOMATIC_OSX
			error_dialog (this, _("Could not start the batch converter.  You may need to download it from dcpomatic.com."));
#else
			error_dialog (this, _("Could not find batch converter."));
#endif
		}
	}

	void jobs_open_dcp_in_player ()
	{
		if (!_film) {
			return;
		}

		if (send_to_other_tool (PLAYER_PLAY_PORT, &start_player, _film->dir(_film->dcp_name(false)).string())) {
#ifdef DCPOMATIC_OSX
			error_dialog (this, _("Could not start the player.  You may need to download it from dcpomatic.com."));
#else
			error_dialog (this, _("Could not find player."));
#endif
		}
	}

	void jobs_make_self_dkdm ()
	{
		if (!_film) {
			return;
		}

		auto d = new SelfDKDMDialog (this, _film);
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


		dcp::LocalTime from (Config::instance()->signer_chain()->leaf().not_before());
		from.add_days (1);
		dcp::LocalTime to (Config::instance()->signer_chain()->leaf().not_after());
		to.add_days (-1);

		optional<dcp::EncryptedKDM> kdm;
		try {
			kdm = _film->make_kdm (
				Config::instance()->decryption_chain()->leaf(),
				vector<string>(),
				d->cpl (),
				from, to,
				dcp::Formulation::MODIFIED_TRANSITIONAL_1,
				true,
				0
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
				auto dkdms = Config::instance()->dkdms();
				dkdms->add (make_shared<DKDM>(kdm.get()));
				Config::instance()->changed ();
			} else {
				auto path = d->directory() / (_film->dcp_name(false) + "_DKDM.xml");
				kdm->as_xml (path);
			}
		}

		d->Destroy ();
	}


	void jobs_export_video_file ()
	{
		auto d = new ExportVideoFileDialog (this, _film->isdcf_name(true));
		if (d->ShowModal() == wxID_OK) {
			if (boost::filesystem::exists(d->path())) {
				bool ok = confirm_dialog(
						this,
						wxString::Format (_("File %s already exists.  Do you want to overwrite it?"), std_to_wx(d->path().string()).data())
						);

				if (!ok) {
					d->Destroy ();
					return;
				}
			}

			auto job = make_shared<TranscodeJob>(_film);
			job->set_encoder (
				make_shared<FFmpegEncoder> (
					_film, job, d->path(), d->format(), d->mixdown_to_stereo(), d->split_reels(), d->split_streams(), d->x264_crf())
				);
			JobManager::instance()->add (job);
		}
		d->Destroy ();
	}


	void jobs_export_subtitles ()
	{
		auto d = new ExportSubtitlesDialog (this, _film->reels().size(), _film->interop());
		if (d->ShowModal() == wxID_OK) {
			auto job = make_shared<TranscodeJob>(_film);
			job->set_encoder (
				make_shared<SubtitleEncoder>(_film, job, d->path(), _film->isdcf_name(true), d->split_reels(), d->include_font())
				);
			JobManager::instance()->add (job);
		}
		d->Destroy ();
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

	void view_closed_captions ()
	{
		_film_viewer->show_closed_captions ();
	}

	void view_video_waveform ()
	{
		if (!_video_waveform_dialog) {
			_video_waveform_dialog = new VideoWaveformDialog (this, _film, _film_viewer);
		}

		_video_waveform_dialog->Show ();
	}

	void tools_system_information ()
	{
		if (!_system_information_dialog) {
			_system_information_dialog = new SystemInformationDialog (this, _film_viewer);
		}

		_system_information_dialog->Show ();
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

	void tools_send_translations ()
	{
		auto d = new SendI18NDialog (this);
		if (d->ShowModal() == wxID_OK) {
			string body;
			body += d->name() + "\n";
			body += d->language() + "\n";
			body += string(dcpomatic_version) + " " + string(dcpomatic_git_commit) + "\n";
			body += "--\n";
			auto translations = I18NHook::translations ();
			for (auto i: translations) {
				body += i.first + "\n" + i.second + "\n\n";
			}
			list<string> to = { "carl@dcpomatic.com" };
			Emailer emailer (d->email(), to, "DCP-o-matic translations", body);
			emailer.send ("main.carlh.net", 2525, EmailProtocol::STARTTLS);
		}

		d->Destroy ();
	}

	void help_about ()
	{
		auto d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void help_report_a_problem ()
	{
		auto d = new ReportProblemDialog (this, _film);
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

		auto d = new wxMessageDialog (
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

			auto dialog = new FilmChangedClosingDialog (_film->name ());
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

		/* Also stop hearing about analytics-related stuff */
		_analytics_message_connection.disconnect ();

		ev.Skip ();
	}

	void set_menu_sensitivity ()
	{
		auto jobs = JobManager::instance()->get ();
		auto i = jobs.begin();
		while (i != jobs.end() && (*i)->json_name() != "transcode") {
			++i;
		}
		bool const dcp_creation = (i != jobs.end ()) && !(*i)->finished ();
		bool const have_cpl = _film && !_film->cpls().empty ();
		bool const have_single_selected_content = _film_editor->content_panel()->selected().size() == 1;
		bool const have_selected_content = !_film_editor->content_panel()->selected().empty();
		bool const have_selected_video_content = !_film_editor->content_panel()->selected_video().empty();

		for (auto j: menu_items) {

			bool enabled = true;

			if ((j.second & NEEDS_FILM) && !_film) {
				enabled = false;
			}

			if ((j.second & NOT_DURING_DCP_CREATION) && dcp_creation) {
				enabled = false;
			}

			if ((j.second & NEEDS_CPL) && !have_cpl) {
				enabled = false;
			}

			if ((j.second & NEEDS_SELECTED_CONTENT) && !have_selected_content) {
				enabled = false;
			}

			if ((j.second & NEEDS_SINGLE_SELECTED_CONTENT) && !have_single_selected_content) {
				enabled = false;
			}

			if ((j.second & NEEDS_SELECTED_VIDEO_CONTENT) && !have_selected_video_content) {
				enabled = false;
			}

			if ((j.second & NEEDS_CLIPBOARD) && !_clipboard) {
				enabled = false;
			}

			if ((j.second & NEEDS_ENCRYPTION) && (!_film || !_film->encrypted())) {
				enabled = false;
			}

			j.first->Enable (enabled);
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
		auto item = menu->Append (id, text);
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

		_file_menu->AppendSeparator ();
		add_item (_file_menu, _("&Close\tCtrl-W"), ID_file_close, NEEDS_FILM);

#ifndef __WXOSX__
		_file_menu->AppendSeparator ();
#endif

#ifdef __WXOSX__
		add_item (_file_menu, _("&Exit"), wxID_EXIT, ALWAYS);
#else
		add_item (_file_menu, _("&Quit"), wxID_EXIT, ALWAYS);
#endif

		auto edit = new wxMenu;
		add_item (edit, _("Copy settings\tCtrl-C"), ID_edit_copy, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SINGLE_SELECTED_CONTENT);
		add_item (edit, _("Paste settings...\tCtrl-V"), ID_edit_paste, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SELECTED_CONTENT | NEEDS_CLIPBOARD);
		edit->AppendSeparator ();
		add_item (edit, _("Select all\tShift-Ctrl-A"), ID_edit_select_all, NEEDS_FILM);

#ifdef __WXOSX__
		add_item (_file_menu, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#else
		edit->AppendSeparator ();
		add_item (edit, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#endif

		auto jobs_menu = new wxMenu;
		add_item (jobs_menu, _("&Make DCP\tCtrl-M"), ID_jobs_make_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		add_item (jobs_menu, _("Make DCP in &batch converter\tCtrl-B"), ID_jobs_make_dcp_batch, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("Make &KDMs...\tCtrl-K"), ID_jobs_make_kdms, NEEDS_FILM);
		add_item (jobs_menu, _("Make &DKDMs...\tCtrl-D"), ID_jobs_make_dkdms, NEEDS_FILM);
		add_item (jobs_menu, _("Make DKDM for DCP-o-matic..."), ID_jobs_make_self_dkdm, NEEDS_FILM | NEEDS_ENCRYPTION);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("Export video file...\tCtrl-E"), ID_jobs_export_video_file, NEEDS_FILM);
		add_item (jobs_menu, _("Export subtitles..."), ID_jobs_export_subtitles, NEEDS_FILM);
		jobs_menu->AppendSeparator ();
		add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);

#if defined(DCPOMATIC_OSX)
		add_item (jobs_menu, _("S&how DCP in Finder"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);
#elif defined(DCPOMATIC_WINDOWS)
		add_item (jobs_menu, _("S&how DCP in Explorer"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);
#else
		add_item (jobs_menu, _("S&how DCP in Files"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);
#endif

		add_item (jobs_menu, _("Open DCP in &player"), ID_jobs_open_dcp_in_player, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_CPL);

		auto view = new wxMenu;
		add_item (view, _("Closed captions..."), ID_view_closed_captions, NEEDS_FILM);
		add_item (view, _("Video waveform..."), ID_view_video_waveform, NEEDS_FILM);

		auto tools = new wxMenu;
		add_item (tools, _("Hints..."), ID_tools_hints, NEEDS_FILM);
		add_item (tools, _("Encoding servers..."), ID_tools_encoding_servers, 0);
		add_item (tools, _("Manage templates..."), ID_tools_manage_templates, 0);
		add_item (tools, _("Check for updates"), ID_tools_check_for_updates, 0);
		add_item (tools, _("Send translations..."), ID_tools_send_translations, 0);
		add_item (tools, _("System information..."), ID_tools_system_information, 0);
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
		m->Append (jobs_menu, _("&Jobs"));
		m->Append (view, _("&View"));
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

		/* Clear out non-existant history items before we re-build the menu */
		Config::instance()->clean_history ();
		auto history = Config::instance()->history();

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

		dcpomatic_log->set_types (Config::instance()->log_types());
	}

	void update_checker_state_changed ()
	{
		auto uc = UpdateChecker::instance ();

		bool const announce =
			_update_news_requested ||
			(uc->stable() && Config::instance()->check_for_updates()) ||
			(uc->test() && Config::instance()->check_for_updates() && Config::instance()->check_for_test_updates());

		_update_news_requested = false;

		if (!announce) {
			return;
		}

		if (uc->state() == UpdateChecker::State::YES) {
			auto dialog = new UpdateDialog (this, uc->stable(), uc->test());
			dialog->ShowModal ();
			dialog->Destroy ();
		} else if (uc->state() == UpdateChecker::State::FAILED) {
			error_dialog (this, _("The DCP-o-matic download server could not be contacted."));
		} else {
			error_dialog (this, _("There are no new versions of DCP-o-matic available."));
		}

		_update_news_requested = false;
	}

	void start_stop_pressed ()
	{
		if (_film_viewer->playing()) {
			_film_viewer->stop();
		} else {
			_film_viewer->start();
		}
	}

	void timeline_pressed ()
	{
		_film_editor->content_panel()->timeline_clicked ();
	}

	void back_frame ()
	{
		_film_viewer->seek_by (-_film_viewer->one_video_frame(), true);
	}

	void forward_frame ()
	{
		_film_viewer->seek_by (_film_viewer->one_video_frame(), true);
	}

	void analytics_message (string title, string html)
	{
		auto d = new HTMLDialog(this, std_to_wx(title), std_to_wx(html));
		d->ShowModal();
		d->Destroy();
	}

	FilmEditor* _film_editor;
	std::shared_ptr<FilmViewer> _film_viewer;
	StandardControls* _controls;
	VideoWaveformDialog* _video_waveform_dialog = nullptr;
	SystemInformationDialog* _system_information_dialog = nullptr;
	HintsDialog* _hints_dialog = nullptr;
	ServersListDialog* _servers_list_dialog = nullptr;
	wxPreferencesEditor* _config_dialog = nullptr;
	KDMDialog* _kdm_dialog = nullptr;
	DKDMDialog* _dkdm_dialog = nullptr;
	TemplatesDialog* _templates_dialog = nullptr;
	wxMenu* _file_menu = nullptr;
	shared_ptr<Film> _film;
	int _history_items = 0;
	int _history_position = 0;
	wxMenuItem* _history_separator = nullptr;
	boost::signals2::scoped_connection _config_changed_connection;
	boost::signals2::scoped_connection _analytics_message_connection;
	bool _update_news_requested = false;
	shared_ptr<Content> _clipboard;
	bool _first_shown_called = false;
};


static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_SWITCH, "n", "new", "create new film", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "content", "add content file / directory", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "d", "dcp", "add content DCP", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "v", "version", "show DCP-o-matic version", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "", "config", "directory containing config.xml and cinemas.xml", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
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
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit ()
	{
		try {
			wxInitAllImageHandlers ();

			Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			_splash = maybe_show_splash ();

			SetAppName (_("DCP-o-matic"));

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			dcpomatic_sleep_seconds (1);
			make_foreground_application ();
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

			/* We only look out for bad configuration from here on, as before
			   dcpomatic_setup() we haven't got OpenSSL ready so there will be
			   incorrect certificate chain validity errors.
			*/
			Config::Bad.connect (boost::bind(&App::config_bad, this, _1));

			_frame = new DOMFrame (_("DCP-o-matic"));
			SetTopWindow (_frame);
			_frame->Maximize ();
			close_splash ();

			if (running_32_on_64 ()) {
				NagDialog::maybe_nag (
					_frame, Config::NAG_32_ON_64,
					_("You are running the 32-bit version of DCP-o-matic on a 64-bit version of Windows.  This will limit the memory available to DCP-o-matic and may cause errors.  You are strongly advised to install the 64-bit version of DCP-o-matic."),
					false);
			}

			_frame->Show ();

			signal_manager = new wxSignalManager (this);
			Bind (wxEVT_IDLE, boost::bind (&App::idle, this, _1));

			if (!_film_to_load.empty() && boost::filesystem::is_directory(_film_to_load)) {
				try {
					_frame->load_film (_film_to_load);
				} catch (exception& e) {
					error_dialog (nullptr, std_to_wx(String::compose(wx_to_std(_("Could not load film %1 (%2)")), _film_to_load)), std_to_wx(e.what()));
				}
			}

			if (!_film_to_create.empty ()) {
				_frame->new_film (_film_to_create, optional<string>());
				if (!_content_to_add.empty()) {
					for (auto i: content_factory(_content_to_add)) {
						_frame->film()->examine_and_add_content(i);
					}
				}
				if (!_dcp_to_add.empty ()) {
					_frame->film()->examine_and_add_content(make_shared<DCPContent>(_dcp_to_add));
				}
			}

			Bind (wxEVT_TIMER, boost::bind (&App::check, this));
			_timer.reset (new wxTimer (this));
			_timer->Start (1000);

			if (Config::instance()->check_for_updates ()) {
				UpdateChecker::instance()->run ();
			}
		}
		catch (exception& e)
		{
			if (_splash) {
				_splash->Destroy ();
				_splash = nullptr;
			}
			error_dialog (nullptr, wxString::Format ("DCP-o-matic could not start."), std_to_wx(e.what()));
		}

		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser)
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser)
	{
		if (parser.Found (wxT("version"))) {
			cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		}

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

		wxString config;
		if (parser.Found (wxT("config"), &config)) {
			State::override_path = wx_to_std (config);
		}

		return true;
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str())
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.path1().string()),
					std_to_wx (e.path2().string())
					)
				);
		} catch (exception& e) {
			error_dialog (
				nullptr,
				wxString::Format(
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

	void idle (wxIdleEvent& ev)
	{
		signal_manager->ui_idle ();
		ev.Skip ();
	}

	void check ()
	{
		try {
			EncodeServerFinder::instance()->rethrow ();
		} catch (exception& e) {
			error_dialog (0, std_to_wx (e.what ()));
		}
	}

	void close_splash ()
	{
		if (_splash) {
			_splash->Destroy ();
			_splash = 0;
		}
	}

	void config_failed_to_load ()
	{
		message_dialog (_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx(m));
	}

	bool config_bad (Config::BadReason reason)
	{
		/* Destroy the splash screen here, as otherwise bad things seem to happen (for reasons unknown)
		   when we open our recreate dialog, close it, *then* try to Destroy the splash (the Destroy fails).
		*/
		_splash->Destroy ();
		_splash = nullptr;

		auto config = Config::instance();
		switch (reason) {
		case Config::BAD_SIGNER_UTF8_STRINGS:
		{
			if (config->nagged(Config::NAG_BAD_SIGNER_CHAIN)) {
				return false;
			}
			auto d = new RecreateChainDialog (
				_frame, _("Recreate signing certificates"),
				_("The certificate chain that DCP-o-matic uses for signing DCPs and KDMs contains a small error\n"
				  "which will prevent DCPs from being validated correctly on some systems.  Do you want to re-create\n"
				  "the certificate chain for signing DCPs and KDMs?"),
				_("Do nothing"),
				Config::NAG_BAD_SIGNER_CHAIN
				);
			int const r = d->ShowModal ();
			d->Destroy ();
			return r == wxID_OK;
		}
		case Config::BAD_SIGNER_INCONSISTENT:
		{
			auto d = new RecreateChainDialog (
				_frame, _("Recreate signing certificates"),
				_("The certificate chain that DCP-o-matic uses for signing DCPs and KDMs is inconsistent and\n"
				  "cannot be used.  DCP-o-matic cannot start unless you re-create it.  Do you want to re-create\n"
				  "the certificate chain for signing DCPs and KDMs?"),
				_("Close DCP-o-matic")
				);
			int const r = d->ShowModal ();
			d->Destroy ();
			if (r != wxID_OK) {
				exit (EXIT_FAILURE);
			}
			return true;
		}
		case Config::BAD_DECRYPTION_INCONSISTENT:
		{
			auto d = new RecreateChainDialog (
				_frame, _("Recreate KDM decryption chain"),
				_("The certificate chain that DCP-o-matic uses for decrypting KDMs is inconsistent and\n"
				  "cannot be used.  DCP-o-matic cannot start unless you re-create it.  Do you want to re-create\n"
				  "the certificate chain for decrypting KDMs?  You may want to say \"No\" here and back up your\n"
				  "configuration before continuing."),
				_("Close DCP-o-matic")
				);
			int const r = d->ShowModal ();
			d->Destroy ();
			if (r != wxID_OK) {
				exit (EXIT_FAILURE);
			}
			return true;
		}
		default:
			DCPOMATIC_ASSERT (false);
		}
	}

	DOMFrame* _frame = nullptr;
	wxSplashScreen* _splash = nullptr;
	shared_ptr<wxTimer> _timer;
	string _film_to_load;
	string _film_to_create;
	string _content_to_add;
	string _dcp_to_add;
};


IMPLEMENT_APP (App)
