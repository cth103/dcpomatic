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


#include "wx/about_dialog.h"
#include "wx/content_panel.h"
#include "wx/dcp_referencing_dialog.h"
#include "wx/dkdm_dialog.h"
#include "wx/export_subtitles_dialog.h"
#include "wx/export_video_file_dialog.h"
#include "wx/film_editor.h"
#include "wx/film_name_location_dialog.h"
#include "wx/film_viewer.h"
#include "wx/focus_manager.h"
#include "wx/full_config_dialog.h"
#include "wx/hints_dialog.h"
#include "wx/html_dialog.h"
#include "wx/file_dialog.h"
#include "wx/i18n_setup.h"
#include "wx/id.h"
#include "wx/job_manager_view.h"
#include "wx/kdm_dialog.h"
#include "wx/load_config_from_zip_dialog.h"
#include "wx/nag_dialog.h"
#include "wx/paste_dialog.h"
#include "wx/recreate_chain_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/save_template_dialog.h"
#include "wx/self_dkdm_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/standard_controls.h"
#include "wx/system_information_dialog.h"
#include "wx/templates_dialog.h"
#include "wx/update_dialog.h"
#include "wx/video_waveform_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/analytics.h"
#include "lib/audio_content.h"
#include "lib/check_content_job.h"
#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_transcode_job.h"
#include "lib/dcpomatic_log.h"
#include "lib/dcpomatic_socket.h"
#include "lib/dkdm_wrapper.h"
#include "lib/email.h"
#include "lib/encode_server_finder.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/film.h"
#include "lib/font_config.h"
#ifdef DCPOMATIC_GROK
#include "lib/grok/context.h"
#endif
#include "lib/hints.h"
#include "lib/job_manager.h"
#include "lib/kdm_with_metadata.h"
#include "lib/log.h"
#include "lib/make_dcp.h"
#include "lib/release_notes.h"
#include "lib/screen.h"
#include "lib/send_kdm_email_job.h"
#include "lib/signal_manager.h"
#include "lib/subtitle_film_encoder.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/unzipper.h"
#include "lib/update_checker.h"
#include "lib/variant.h"
#include "lib/version.h"
#include "lib/video_content.h"
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/scope_guard.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/cmdline.h>
#include <wx/generic/aboutdlgg.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/wxhtml.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
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
using boost::optional;
using boost::is_any_of;
using boost::algorithm::find;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


class FilmChangedClosingDialog
{
public:
	explicit FilmChangedClosingDialog (string name)
		: _dialog(
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before closing?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			)
	{
		_dialog.SetYesNoCancelLabels(
			_("Save film and close"), _("Close without saving film"), _("Don't close")
			);
	}

	int run ()
	{
		return _dialog.ShowModal();
	}

private:
	wxMessageDialog _dialog;
};


class FilmChangedDuplicatingDialog
{
public:
	explicit FilmChangedDuplicatingDialog (string name)
		: _dialog(
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before duplicating?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			)
	{
		_dialog.SetYesNoCancelLabels(
			_("Save film and duplicate"), _("Duplicate without saving film"), _("Don't duplicate")
			);
	}

	int run ()
	{
		return _dialog.ShowModal();
	}

private:
	wxMessageDialog _dialog;
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
#define NEEDS_DCP_CONTENT             0x100


map<wxMenuItem*, int> menu_items;

enum {
	ID_file_new = DCPOMATIC_MAIN_MENU,
	ID_file_open,
	ID_file_save,
	ID_file_save_as_template,
	ID_file_duplicate,
	ID_file_duplicate_and_open,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_file_close = DCPOMATIC_MAIN_MENU + 100,
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
	ID_tools_version_file,
	ID_tools_hints,
	ID_tools_encoding_servers,
	ID_tools_manage_templates,
	ID_tools_check_for_updates,
	ID_tools_system_information,
	ID_tools_restore_default_preferences,
	ID_tools_export_preferences,
	ID_tools_import_preferences,
	ID_help_report_a_problem,
	/* IDs for shortcuts (with no associated menu item) */
	ID_add_file,
	ID_remove,
	ID_start_stop,
	ID_timeline,
	ID_back_frame,
	ID_forward_frame
};


class LimitedFrameSplitter : public wxSplitterWindow
{
public:
	LimitedFrameSplitter(wxWindow* parent)
		: wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_NOBORDER | wxSP_3DSASH | wxSP_LIVE_UPDATE)
	{
		/* This value doesn't really mean much but we just want to stop double-click on the
		 * divider from shrinking the left panel.
		 */
		SetMinimumPaneSize(64);

		Bind(wxEVT_SIZE, boost::bind(&LimitedFrameSplitter::sized, this, _1));
	}

	bool OnSashPositionChange(int new_position) override
	{
		/* Try to stop the left bit of the splitter getting too small */
		auto const ok = new_position > _left_panel_minimum_size;
		if (ok) {
			Config::instance()->set_main_divider_sash_position(new_position);
		}
		return ok;
	}

private:
	void sized(wxSizeEvent& ev)
	{
		if (GetSize().GetWidth() > _left_panel_minimum_size && GetSashPosition() < _left_panel_minimum_size) {
			/* The window is now fairly big but the left panel is small; this happens when the DCP-o-matic window
			 * is shrunk and then made larger again.  Try to set a sensible left panel size in this case.
			 */
			SetSashPosition(Config::instance()->main_divider_sash_position().get_value_or(_left_panel_minimum_size));
		}

		ev.Skip();
	}

	int const _left_panel_minimum_size = 200;
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const& title)
		: wxFrame (nullptr, -1, title)
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		, _splitter(new LimitedFrameSplitter(this))
		, _right_panel(new wxPanel(_splitter, wxID_ANY))
		, _film_viewer(_right_panel, false)
	{
		auto bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

#ifdef DCPOMATIC_WINDOWS
		SetIcon (wxIcon (std_to_wx ("id")));
#endif

		_config_changed_connection = Config::instance()->Changed.connect(boost::bind(&DOMFrame::config_changed, this, _1));
		config_changed(Config::OTHER);

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
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_version_file, this),      ID_tools_version_file);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_hints, this),             ID_tools_hints);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_encoding_servers, this),  ID_tools_encoding_servers);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_manage_templates, this),  ID_tools_manage_templates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_system_information, this),ID_tools_system_information);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_restore_default_preferences, this), ID_tools_restore_default_preferences);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_export_preferences, this), ID_tools_export_preferences);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_import_preferences, this), ID_tools_import_preferences);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),              wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this),   ID_help_report_a_problem);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&DOMFrame::close, this, _1));
		Bind (wxEVT_SHOW, boost::bind (&DOMFrame::show, this, _1));

		auto left_panel = new wxPanel(_splitter, wxID_ANY);

		_film_editor = new FilmEditor(left_panel, _film_viewer);

		auto left_sizer = new wxBoxSizer(wxHORIZONTAL);
		left_sizer->Add(_film_editor, 1, wxEXPAND);

		left_panel->SetSizerAndFit(left_sizer);

		_controls = new StandardControls(_right_panel, _film_viewer, true);
		_controls->set_film(_film_viewer.film());
		auto job_manager_view = new JobManagerView(_right_panel, false);

		auto right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add(_film_viewer.panel(), 2, wxEXPAND | wxALL, 6);
		right_sizer->Add (_controls, 0, wxEXPAND | wxALL, 6);
		right_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL, 6);

		_right_panel->SetSizer(right_sizer);

		_splitter->SplitVertically(left_panel, _right_panel, Config::instance()->main_divider_sash_position().get_value_or(left_panel->GetSize().GetWidth() + 8));

		set_menu_sensitivity ();

		_film_editor->content_panel()->SelectionChanged.connect (boost::bind (&DOMFrame::set_menu_sensitivity, this));
		set_title ();

		JobManager::instance()->ActiveJobsChanged.connect(boost::bind(&DOMFrame::active_jobs_changed, this));

		UpdateChecker::instance()->StateChanged.connect(boost::bind(&DOMFrame::update_checker_state_changed, this));

		FocusManager::instance()->SetFocus.connect (boost::bind (&DOMFrame::remove_accelerators, this));
		FocusManager::instance()->KillFocus.connect (boost::bind (&DOMFrame::add_accelerators, this));
		add_accelerators ();
	}

	~DOMFrame()
	{
		/* This holds a reference to FilmViewer, so get rid of it first */
		_video_waveform_dialog.reset();
	}

	void add_accelerators ()
	{
#ifdef __WXOSX__
		int accelerators = 7;
#else
		int accelerators = 6;
#endif
		std::vector<wxAcceleratorEntry> accel(accelerators);
		/* [Shortcut] Ctrl+A:Add file(s) to the film */
		accel[0].Set (wxACCEL_CTRL, static_cast<int>('A'), ID_add_file);
		/* [Shortcut] Delete:Remove selected content from film */
		accel[1].Set (wxACCEL_NORMAL, WXK_DELETE, ID_remove);
		/* [Shortcut] Space:Start/stop playback */
		accel[2].Set (wxACCEL_NORMAL, WXK_SPACE, ID_start_stop);
		/* [Shortcut] Ctrl+T:Open timeline window */
		accel[3].Set (wxACCEL_CTRL, static_cast<int>('T'), ID_timeline);
		/* [Shortcut] Left arrow:Move back one frame */
		accel[4].Set (wxACCEL_NORMAL, WXK_LEFT, ID_back_frame);
		/* [Shortcut] Right arrow:Move forward one frame */
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
		wxAcceleratorTable accel_table (accelerators, accel.data());
		SetAcceleratorTable (accel_table);
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

	/** Make a new film in the given path, using template_name as a template
	 *  (or the default template if it's empty).
	 */
	void new_film (boost::filesystem::path path, optional<string> template_name)
	{
		auto film = make_shared<Film>(path);
		film->use_template(template_name);
		film->set_name (path.filename().generic_string());
		film->write_metadata ();
		set_film (film);
	}

	void load_film (boost::filesystem::path file)
	try
	{
		auto film = make_shared<Film>(file);
		auto const notes = film->read_metadata ();
		film->read_ui_state();

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

		JobManager::instance()->add(make_shared<CheckContentJob>(film));
	}
	catch (FileNotFoundError& e) {
		auto const dir = e.file().parent_path();
		if (dcp::filesystem::exists(dir / "ASSETMAP") || dcp::filesystem::exists(dir / "ASSETMAP.xml")) {
			error_dialog (
				this, variant::wx::insert_dcpomatic(_("Could not open this folder as a %s project.")),
				variant::wx::insert_dcpomatic(
					_("It looks like you are trying to open a DCP.  File -> Open is for loading %s projects, not DCPs.  "
					  "To import a DCP, create a new project with File -> New and then click the \"Add DCP...\" button."))
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
		_film_viewer.set_film(_film);
		_film_editor->set_film(_film);
		_controls->set_film (_film);
		_video_waveform_dialog.reset();
		set_menu_sensitivity ();
		if (_film && _film->directory()) {
			Config::instance()->add_to_history (_film->directory().get());
		}
		if (_film) {
			_film->Change.connect (boost::bind (&DOMFrame::film_change, this, _1));
			_film->Message.connect (boost::bind(&DOMFrame::film_message, this, _1));
			_film->DirtyChange.connect (boost::bind(&DOMFrame::set_title, this));
			dcpomatic_log = _film->log ();
		}
		set_title ();
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
#ifdef DCPOMATIC_WORKAROUND_MUTTER
			signal_manager->when_idle([this]() { Maximize(); });
#endif
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

	void file_new ()
	{
		FilmNameLocationDialog dialog(this, _("New Film"), true);
		int const r = dialog.ShowModal();

		if (r != wxID_OK || !dialog.check_path() || !maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			return;
		}

		try {
			new_film(dialog.path(), dialog.template_name());
		} catch (boost::filesystem::filesystem_error& e) {
#ifdef DCPOMATIC_WINDOWS
			string bad_chars = "<>:\"/|?*";
			string const filename = dialog.path().filename().string();
			string found_bad_chars;
			for (size_t i = 0; i < bad_chars.length(); ++i) {
				if (filename.find(bad_chars[i]) != string::npos && found_bad_chars.find(bad_chars[i]) == string::npos) {
					found_bad_chars += bad_chars[i];
				}
			}
			wxString message = _("Could not create folder to store film.");
			message += char_to_wx("  ");
			if (!found_bad_chars.empty()) {
				message += wxString::Format (_("Try removing the %s characters from your folder name."), std_to_wx(found_bad_chars).data());
			} else {
				message += variant::wx::insert_dcpomatic(_("Please check that you do not have Windows controlled folder access enabled for %s."));
			}
			error_dialog (this, message, std_to_wx(e.what()));
#else
			error_dialog (this, _("Could not create folder to store film."), std_to_wx(e.what()));
#endif
		}
	}

	void file_open ()
	{
		wxDirDialog dialog(
			this,
			_("Select film to open"),
			std_to_wx (Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()),
			wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST
			);

		int r;
		while (true) {
			r = dialog.ShowModal();
			if (r == wxID_OK && dialog.GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK && maybe_save_then_delete_film<FilmChangedClosingDialog>()) {
			load_film(wx_to_std(dialog.GetPath()));
		}
	}

	void file_save ()
	{
		try {
			_film->write_metadata ();
		} catch (exception& e) {
			error_dialog(this, _("Could not save project."), std_to_wx(e.what()));
		}
	}

	void file_save_as_template ()
	{
		SaveTemplateDialog dialog(this);
		if (dialog.ShowModal() == wxID_OK) {
			try {
				if (dialog.name()) {
					Config::instance()->save_template(_film, *dialog.name());
				} else {
					Config::instance()->save_default_template(_film);
				}
			} catch (exception& e) {
				error_dialog(this, _("Could not save template."), std_to_wx(e.what()));
			}
		}
	}

	void file_duplicate ()
	{
		FilmNameLocationDialog dialog(this, _("Duplicate Film"), false);

		if (dialog.ShowModal() == wxID_OK && dialog.check_path() && maybe_save_film<FilmChangedDuplicatingDialog>()) {
			auto film = make_shared<Film>(dialog.path());
			film->copy_from (_film);
			film->set_name(dialog.path().filename().generic_string());
			try {
				film->write_metadata();
			} catch (exception& e) {
				error_dialog(this, _("Could not duplicate project."), std_to_wx(e.what()));
			}
		}
	}

	void file_duplicate_and_open ()
	{
		FilmNameLocationDialog dialog(this, _("Duplicate Film"), false);

		if (dialog.ShowModal() == wxID_OK && dialog.check_path() && maybe_save_film<FilmChangedDuplicatingDialog>()) {
			auto film = make_shared<Film>(dialog.path());
			film->copy_from (_film);
			film->set_name(dialog.path().filename().generic_string());
			try {
				film->write_metadata ();
				set_film (film);
			} catch (exception& e) {
				error_dialog(this, _("Could not duplicate project."), std_to_wx(e.what()));
			}
		}
	}

	void file_close ()
	{
		if (_film && _film->dirty ()) {
			FilmChangedClosingDialog dialog(_film->name());
			switch (dialog.run()) {
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
		if (sel.size() == 1) {
			_clipboard = sel.front()->clone();
		}
	}

	void edit_paste ()
	{
		if (!_clipboard) {
			return;
		}

		PasteDialog dialog(this, static_cast<bool>(_clipboard->video), static_cast<bool>(_clipboard->audio), !_clipboard->text.empty());
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}

		for (auto i: _film_editor->content_panel()->selected()) {
			if (dialog.video() && i->video) {
				DCPOMATIC_ASSERT (_clipboard->video);
				i->video->take_settings_from (_clipboard->video);
			}
			if (dialog.audio() && i->audio) {
				DCPOMATIC_ASSERT (_clipboard->audio);
				i->audio->take_settings_from (_clipboard->audio);
			}

			if (dialog.text()) {
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
		wxMessageDialog dialog(
			nullptr,
			_("Are you sure you want to restore preferences to their defaults?  This cannot be undone."),
			_("Restore default preferences"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		if (dialog.ShowModal() == wxID_YES) {
			Config::restore_defaults ();
		}
	}

	void tools_export_preferences ()
	{
		FileDialog dialog(
			this, _("Specify ZIP file"), char_to_wx("ZIP files (*.zip)|*.zip"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT, "Preferences", string("dcpomatic_config.zip")
			);

		if (dialog.ShowModal() == wxID_OK) {
			auto const path = boost::filesystem::path(wx_to_std(dialog.GetPath()));
			if (boost::filesystem::exists(path)) {
				boost::system::error_code ec;
				boost::filesystem::remove(path, ec);
				if (ec) {
					error_dialog(nullptr, _("Could not remove existing preferences file"), std_to_wx(path.string()));
					return;
				}
			}

			save_all_config_as_zip(path);
		}
	}

	void tools_import_preferences()
	{
		FileDialog dialog(this, _("Specify ZIP file"), char_to_wx("ZIP files (*.zip)|*.zip"), wxFD_OPEN, "Preferences");

		if (!dialog.show()) {
			return;
		}

		auto action = Config::CinemasAction::WRITE_TO_CURRENT_PATH;

		if (Config::zip_contains_cinemas(dialog.path()) && Config::cinemas_file_from_zip(dialog.path()) != Config::instance()->cinemas_file()) {
			LoadConfigFromZIPDialog how(this, dialog.path());
			if (how.ShowModal() == wxID_CANCEL) {
				return;
			}

			action = how.action();
		}

		Config::instance()->load_from_zip(dialog.path(), action);
	}

	void jobs_make_dcp ()
	{
		double required;
		double available;

		if (!_film->should_be_enough_disk_space(required, available)) {
			auto const message = wxString::Format(_("The DCP for this film will take up about %.1f GB, and the disk that you are using only has %.1f GB available.  Do you want to continue anyway?"), required, available);
			if (!confirm_dialog (this, message)) {
				return;
			}
		}

		if (Config::instance()->show_hints_before_make_dcp()) {
			HintsDialog hints(this, _film, false);
			if (hints.ShowModal() == wxID_CANCEL) {
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
		if (dcp::filesystem::exists(dcp_dir)) {
			if (!confirm_dialog (this, wxString::Format (_("Do you want to overwrite the existing DCP %s?"), std_to_wx(dcp_dir.string()).data()))) {
				return;
			}

			preserve_assets(dcp_dir, _film->assets_path());
			dcp::filesystem::remove_all(dcp_dir);
		}

		try {
			/* It seems to make sense to auto-save metadata here, since the make DCP may last
			   a long time, and crashes/power failures are moderately likely.
			*/
			_film->write_metadata ();
			make_dcp (_film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
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

		_kdm_dialog.reset(this, _film);
		_kdm_dialog->Show ();
	}

	void jobs_make_dkdms ()
	{
		if (!_film) {
			return;
		}

		_dkdm_dialog.reset(this, _film);
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
				Socket socket (5);
				socket.connect("127.0.0.1", port);
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
			HintsDialog hints(this, _film, false);
			if (hints.ShowModal() == wxID_CANCEL) {
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

		SelfDKDMDialog dialog(this, _film);
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}

		NagDialog::maybe_nag (
			this,
			Config::NAG_DKDM_CONFIG,
			wxString::Format (
				_("You are making a DKDM which is encrypted by a private key held in"
				  "\n\n<tt>%s</tt>\n\nIt is <span weight=\"bold\" size=\"larger\">VITALLY IMPORTANT</span> "
				  "that you <span weight=\"bold\" size=\"larger\">BACK UP THIS FILE</span> since if it is lost "
				  "your DKDMs (and the DCPs they protect) will become useless."), std_to_wx(Config::config_read_file().string()).data()
				)
			);


		dcp::LocalTime from (Config::instance()->signer_chain()->leaf().not_before());
		from.add_days (1);
		dcp::LocalTime to (Config::instance()->signer_chain()->leaf().not_after());
		to.add_days (-1);

		auto signer = Config::instance()->signer_chain();
		if (!signer->valid()) {
			error_dialog(this, _("The certificate chain for signing is invalid"));
			return;
		}

		optional<dcp::EncryptedKDM> kdm;
		try {
			auto const decrypted_kdm = _film->make_kdm(dialog.cpl(), from, to);
			auto const kdm = decrypted_kdm.encrypt(signer, Config::instance()->decryption_chain()->leaf(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, true, 0);
			if (dialog.internal()) {
				auto dkdms = Config::instance()->dkdms();
				dkdms->add(make_shared<DKDM>(kdm));
				Config::instance()->changed ();
			} else {
				auto path = dialog.directory() / (_film->dcp_name(false) + "_DKDM.xml");
				kdm.as_xml(path);
			}
		} catch (dcp::NotEncryptedError& e) {
			error_dialog (this, _("CPL's content is not encrypted."));
		} catch (exception& e) {
			error_dialog(this, std_to_wx(e.what()));
		} catch (...) {
			error_dialog (this, _("An unknown exception occurred."));
		}
	}


	void jobs_export_video_file ()
	{
		ExportVideoFileDialog dialog(this, _film->isdcf_name(true));
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}

		if (dcp::filesystem::exists(dialog.path())) {
			bool ok = confirm_dialog(
					this,
					wxString::Format(_("File %s already exists.  Do you want to overwrite it?"), std_to_wx(dialog.path().string()).data())
					);

			if (!ok) {
				return;
			}
		}

		auto job = make_shared<TranscodeJob>(_film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
		job->set_encoder (
			make_shared<FFmpegFilmEncoder>(
				_film, job, dialog.path(), dialog.format(), dialog.mixdown_to_stereo(), dialog.split_reels(), dialog.split_streams(), dialog.x264_crf())
			);
		JobManager::instance()->add (job);
	}


	void jobs_export_subtitles ()
	{
		ExportSubtitlesDialog dialog(this, _film->reels().size(), _film->interop());
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}
		auto job = make_shared<TranscodeJob>(_film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
		job->set_encoder(
			make_shared<SubtitleFilmEncoder>(
				_film,
				job,
				dialog.path(),
				_film->isdcf_name(true),
				dialog.split_reels(),
				dialog.include_font(),
				dialog.standard()
			)
		);
		JobManager::instance()->add(job);
	}


	void jobs_send_dcp_to_tms ()
	{
		_film->send_dcp_to_tms ();
	}

	void jobs_show_dcp ()
	{
		DCPOMATIC_ASSERT (_film->directory ());
		if (show_in_file_manager(_film->directory().get(), _film->dir(_film->dcp_name(false)))) {
			error_dialog (this, _("Could not show DCP."));
		}
	}

	void view_closed_captions ()
	{
		_film_viewer.show_closed_captions ();
	}

	void view_video_waveform ()
	{
		if (!_video_waveform_dialog) {
			_video_waveform_dialog.reset(this, _film, _film_viewer);
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

	void tools_version_file()
	{
		if (_dcp_referencing_dialog) {
			_dcp_referencing_dialog->Destroy();
			_dcp_referencing_dialog = nullptr;
		}

		_dcp_referencing_dialog = new DCPReferencingDialog(this, _film);
		_dcp_referencing_dialog->Show();
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
			_templates_dialog.reset(this);
		}

		_templates_dialog->Show ();
	}

	void tools_check_for_updates ()
	{
		_update_news_requested = true;
		UpdateChecker::instance()->run();
	}

	void help_about ()
	{
		AboutDialog dialog(this);
		dialog.ShowModal();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog dialog(this, _film);
		if (dialog.ShowModal() == wxID_OK) {
			dialog.report();
		}
	}

	bool should_close ()
	{
		if (!JobManager::instance()->work_to_do ()) {
			return true;
		}

		wxMessageDialog dialog(
			nullptr,
			_("There are unfinished jobs; are you sure you want to quit?"),
			_("Unfinished jobs"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		return dialog.ShowModal() == wxID_YES;
	}

	void close (wxCloseEvent& ev)
	{
		if (!should_close ()) {
			ev.Veto ();
			return;
		}

		if (_film && _film->dirty ()) {
			FilmChangedClosingDialog dialog(_film->name());
			switch (dialog.run()) {
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

		FontConfig::drop();

		ev.Skip ();
		JobManager::drop ();
	}

	void active_jobs_changed()
	{
		/* ActiveJobsChanged can be called while JobManager holds a lock on its mutex, making
		 * the call to JobManager::get() in set_menu_sensitivity() deadlock unless we work around
		 * it by using an idle callback.  This feels quite unpleasant.
		 */
		signal_manager->when_idle(boost::bind(&DOMFrame::set_menu_sensitivity, this));
	}

	void set_menu_sensitivity ()
	{
		auto jobs = JobManager::instance()->get ();
		auto const dcp_creation = std::any_of(
			jobs.begin(),
			jobs.end(),
			[](shared_ptr<const Job> job) {
				return dynamic_pointer_cast<const DCPTranscodeJob>(job) && !job->finished();
			});
		bool const have_cpl = _film && !_film->cpls().empty ();
		bool const have_single_selected_content = _film_editor->content_panel()->selected().size() == 1;
		bool const have_selected_content = !_film_editor->content_panel()->selected().empty();
		bool const have_selected_video_content = !_film_editor->content_panel()->selected_video().empty();
		vector<shared_ptr<Content>> content;
		if (_film) {
			content = _film->content();
		}
		bool const have_dcp_content = std::find_if(content.begin(), content.end(), [](shared_ptr<const Content> content) {
			return static_cast<bool>(dynamic_pointer_cast<const DCPContent>(content));
		}) != content.end();

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

			if ((j.second & NEEDS_DCP_CONTENT) && !have_dcp_content) {
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

		while (_film->dirty()) {
			T dialog(_film->name());
			switch (dialog.run()) {
			case wxID_NO:
				return true;
			case wxID_YES:
				try {
					_film->write_metadata();
					return true;
				} catch (exception& e) {
					error_dialog(this, _("Could not save project."), std_to_wx(e.what()));
					/* Go round again for another try */
				}
				break;
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
		/* [Shortcut] Ctrl+N:New film */
		add_item (_file_menu, _("New...\tCtrl-N"), ID_file_new, ALWAYS);
		/* [Shortcut] Ctrl+O:Open existing film */
		add_item (_file_menu, _("&Open...\tCtrl-O"), ID_file_open, ALWAYS);
		_file_menu->AppendSeparator ();
		/* [Shortcut] Ctrl+S:Save current film */
		add_item (_file_menu, _("&Save\tCtrl-S"), ID_file_save, NEEDS_FILM);
		_file_menu->AppendSeparator ();
		add_item (_file_menu, _("Save as &template..."), ID_file_save_as_template, NEEDS_FILM);
		add_item (_file_menu, _("Duplicate..."), ID_file_duplicate, NEEDS_FILM);
		add_item (_file_menu, _("Duplicate and open..."), ID_file_duplicate_and_open, NEEDS_FILM);

		_history_position = _file_menu->GetMenuItems().GetCount();

		_file_menu->AppendSeparator ();
		/* [Shortcut] Ctrl+W:Close current film */
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
		/* [Shortcut] Ctrl+C:Copy settings from currently selected content */
		add_item (edit, _("Copy settings\tCtrl-C"), ID_edit_copy, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SINGLE_SELECTED_CONTENT);
		/* [Shortcut] Ctrl+V:Paste settings into currently selected content */
		add_item (edit, _("Paste settings...\tCtrl-V"), ID_edit_paste, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_SELECTED_CONTENT | NEEDS_CLIPBOARD);
		edit->AppendSeparator ();
		/* [Shortcut] Shift+Ctrl+A:Select all content */
		add_item (edit, _("Select all\tShift-Ctrl-A"), ID_edit_select_all, NEEDS_FILM);

#ifdef __WXOSX__
		add_item(_file_menu, _("&Preferences...\tCtrl-,"), wxID_PREFERENCES, ALWAYS);
#else
		edit->AppendSeparator ();
		/* [Shortcut] Ctrl+P:Open preferences window */
		add_item (edit, _("&Preferences...\tCtrl-P"), wxID_PREFERENCES, ALWAYS);
#endif

		auto jobs_menu = new wxMenu;
		/* [Shortcut] Ctrl+M:Make DCP */
		add_item (jobs_menu, _("&Make DCP\tCtrl-M"), ID_jobs_make_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		/* [Shortcut] Ctrl+B:Make DCP in the batch converter*/
		add_item (jobs_menu, _("Make DCP in &batch converter\tCtrl-B"), ID_jobs_make_dcp_batch, NEEDS_FILM | NOT_DURING_DCP_CREATION);
		jobs_menu->AppendSeparator ();
		/* [Shortcut] Ctrl+K:Make KDMs */
		add_item (jobs_menu, _("Make &KDMs...\tCtrl-K"), ID_jobs_make_kdms, NEEDS_FILM);
		/* [Shortcut] Ctrl+D:Make DKDMs */
		add_item (jobs_menu, _("Make &DKDMs...\tCtrl-D"), ID_jobs_make_dkdms, NEEDS_FILM);
		add_item(jobs_menu, variant::wx::insert_dcpomatic(_("Make DKDM for %s...")), ID_jobs_make_self_dkdm, NEEDS_FILM | NEEDS_ENCRYPTION);
		jobs_menu->AppendSeparator ();
		/* [Shortcut] Ctrl+E:Export video file */
		add_item (jobs_menu, _("Export video file...\tCtrl-E"), ID_jobs_export_video_file, NEEDS_FILM);
		add_item (jobs_menu, _("Export subtitles...\tShift-Ctrl-E"), ID_jobs_export_subtitles, NEEDS_FILM);
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
		add_item (tools, _("Version File (VF)..."), ID_tools_version_file, NEEDS_FILM | NEEDS_DCP_CONTENT);
		add_item (tools, _("Hints..."), ID_tools_hints, NEEDS_FILM);
		add_item (tools, _("Encoding servers..."), ID_tools_encoding_servers, 0);
		add_item (tools, _("Manage templates..."), ID_tools_manage_templates, 0);
		add_item (tools, _("Check for updates"), ID_tools_check_for_updates, 0);
		add_item (tools, _("System information..."), ID_tools_system_information, 0);
		tools->AppendSeparator ();
		add_item (tools, _("Restore default preferences"), ID_tools_restore_default_preferences, ALWAYS);
		tools->AppendSeparator ();
		add_item (tools, _("Export preferences..."), ID_tools_export_preferences, ALWAYS);
		add_item (tools, _("Import preferences..."), ID_tools_import_preferences, ALWAYS);

		wxMenu* help = new wxMenu;
#ifdef __WXOSX__
		add_item(help, variant::wx::insert_dcpomatic(_("About %s")), wxID_ABOUT, ALWAYS);
#else
		add_item (help, _("About"), wxID_ABOUT, ALWAYS);
#endif
		if (variant::show_report_a_problem()) {
			add_item(help, _("Report a problem..."), ID_help_report_a_problem, ALWAYS);
		}

		m->Append (_file_menu, _("&File"));
		m->Append (edit, _("&Edit"));
		m->Append (jobs_menu, _("&Jobs"));
		m->Append (view, _("&View"));
		m->Append (tools, _("&Tools"));
		m->Append (help, _("&Help"));
	}

	void config_changed(Config::Property what)
	{
		/* Instantly save any config changes when using the DCP-o-matic GUI */
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

		for (int i = 0; i < _history_items; ++i) {
			delete _file_menu->Remove (ID_file_history + i);
		}

		if (_history_separator) {
			_file_menu->Remove (_history_separator);
		}
		delete _history_separator;
		_history_separator = 0;

		int pos = _history_position;

		/* Clear out non-existent history items before we re-build the menu */
		Config::instance()->clean_history ();
		auto history = Config::instance()->history();

		if (!history.empty ()) {
			_history_separator = _file_menu->InsertSeparator (pos++);
		}

		for (size_t i = 0; i < history.size(); ++i) {
			string s;
			if (i < 9) {
				s = fmt::format("&{} {}", i + 1, history[i].string());
			} else {
				s = history[i].string();
			}
			_file_menu->Insert (pos++, ID_file_history + i, std_to_wx (s));
		}

		_history_items = history.size ();

		dcpomatic_log->set_types (Config::instance()->log_types());

#ifdef DCPOMATIC_GROK
		if (what == Config::GROK) {
			setup_grok_library_path();
		}
#else
		LIBDCP_UNUSED(what);
#endif
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
			UpdateDialog dialog(this, uc->stable(), uc->test());
			dialog.ShowModal();
		} else if (uc->state() == UpdateChecker::State::FAILED) {
			error_dialog(this, variant::wx::insert_dcpomatic(_("The %s download server could not be contacted.")));
		} else {
			error_dialog(this, variant::wx::insert_dcpomatic(_("There are no new versions of %s available.")));
		}

		_update_news_requested = false;
	}

	void start_stop_pressed ()
	{
		if (_film_viewer.playing()) {
			_film_viewer.stop();
		} else {
			_film_viewer.start();
		}
	}

	void timeline_pressed ()
	{
		_film_editor->content_panel()->timeline_clicked ();
	}

	void back_frame ()
	{
		_film_viewer.seek_by(-_film_viewer.one_video_frame(), true);
	}

	void forward_frame ()
	{
		_film_viewer.seek_by(_film_viewer.one_video_frame(), true);
	}

	void analytics_message (string title, string html)
	{
		HTMLDialog dialog(this, std_to_wx(title), std_to_wx(html));
		dialog.ShowModal();
	}

	void set_title ()
	{
		auto s = variant::dcpomatic();
		if (_film) {
			if (_film->directory()) {
				s += " - " + _film->directory()->string();
			}
			if (_film->dirty()) {
				s += " *";
			}
		}

		SetTitle (std_to_wx(s));
	}

	FilmEditor* _film_editor;
	LimitedFrameSplitter* _splitter;
	wxPanel* _right_panel;
	FilmViewer _film_viewer;
	StandardControls* _controls;
	wx_ptr<VideoWaveformDialog> _video_waveform_dialog;
	SystemInformationDialog* _system_information_dialog = nullptr;
	DCPReferencingDialog* _dcp_referencing_dialog = nullptr;
	HintsDialog* _hints_dialog = nullptr;
	ServersListDialog* _servers_list_dialog = nullptr;
	wxPreferencesEditor* _config_dialog = nullptr;
	wx_ptr<KDMDialog> _kdm_dialog;
	wx_ptr<DKDMDialog> _dkdm_dialog;
	wx_ptr<TemplatesDialog> _templates_dialog;
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
	{ wxCMD_LINE_SWITCH, "v", "version", "show version", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
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
		dcpomatic_setup_path_encoding ();
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit () override
	{
		try {

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

			cout << variant::insert_dcpomatic("{} is starting.") << "\n";
		}
#endif
			wxInitAllImageHandlers ();

			Config::FailedToLoad.connect(boost::bind(&App::config_failed_to_load, this, _1));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			_splash = maybe_show_splash ();

			SetAppName(variant::wx::dcpomatic());

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

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic::wx::setup_i18n();

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

			signal_manager = new wxSignalManager (this);

			_frame = new DOMFrame(variant::wx::dcpomatic());
			SetTopWindow (_frame);
			_frame->Maximize ();
			close_splash ();

			if (running_32_on_64 ()) {
				NagDialog::maybe_nag (
					_frame, Config::NAG_32_ON_64,
					wxString::Format(
						_("You are running the 32-bit version of %s on a 64-bit version of Windows.  "
						  "This will limit the memory available to %s and may cause errors.  You are "
						  "strongly advised to install the 64-bit version of %s."),
						variant::wx::dcpomatic(),
						variant::wx::dcpomatic(),
						variant::wx::dcpomatic()
						),
					false);
			}

			_frame->Show ();

			Bind (wxEVT_IDLE, boost::bind (&App::idle, this, _1));

			if (!_film_to_load.empty() && dcp::filesystem::is_directory(_film_to_load)) {
				try {
					_frame->load_film (_film_to_load);
				} catch (exception& e) {
					error_dialog (nullptr, std_to_wx(fmt::format(wx_to_std(_("Could not load film {} ({})")), _film_to_load)), std_to_wx(e.what()));
				}
			}

			if (!_film_to_create.empty ()) {
				_frame->new_film (_film_to_create, optional<string>());
				if (!_content_to_add.empty()) {
					_frame->film()->examine_and_add_content(content_factory(_content_to_add));
				}
				if (!_dcp_to_add.empty ()) {
					_frame->film()->examine_and_add_content({make_shared<DCPContent>(_dcp_to_add)});
				}
			}

			Bind (wxEVT_TIMER, boost::bind (&App::check, this));
			_timer.reset (new wxTimer (this));
			_timer->Start (1000);

			if (Config::instance()->check_for_updates ()) {
				UpdateChecker::instance()->run ();
			}

			if (auto release_notes = find_release_notes(gui_is_dark())) {
				HTMLDialog notes(nullptr, _("Release notes"), std_to_wx(*release_notes), true);
				notes.Centre();
				notes.ShowModal();
			}

#ifdef DCPOMATIC_GROK
			grk_plugin::setMessengerLogger(new grk_plugin::GrokLogger("[GROK] "));
			setup_grok_library_path();
#endif
		}
		catch (exception& e)
		{
			close_splash();
			error_dialog(nullptr, variant::wx::insert_dcpomatic(_("%s could not start.")), std_to_wx(e.what()));
		}

		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars(char_to_wx("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
	{
		if (parser.Found(char_to_wx("version"))) {
			cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		}

		if (parser.GetParamCount() > 0) {
			if (parser.Found(char_to_wx("new"))) {
				_film_to_create = wx_to_std (parser.GetParam (0));
			} else {
				_film_to_load = wx_to_std (parser.GetParam (0));
			}
		}

		wxString content;
		if (parser.Found(char_to_wx("content"), &content)) {
			_content_to_add = wx_to_std (content);
		}

		wxString dcp;
		if (parser.Found(char_to_wx("dcp"), &dcp)) {
			_dcp_to_add = wx_to_std (dcp);
		}

		wxString config;
		if (parser.Found(char_to_wx("config"), &config)) {
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
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.path1().string()),
					std_to_wx(e.path2().string()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n%s"),
					std_to_wx(e.what()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), dcpomatic::wx::report_problem()));
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop () override
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
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
			_splash->Destroy();
			_splash = nullptr;
		}
	}

	void config_failed_to_load (Config::LoadFailure what)
	{
		report_config_load_failure(_frame, what);
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
		close_splash();

		auto config = Config::instance();
		switch (reason) {
		case Config::BAD_SIGNER_UTF8_STRINGS:
		{
			if (config->nagged(Config::NAG_BAD_SIGNER_CHAIN_UTF8)) {
				return false;
			}
			RecreateChainDialog dialog(
				_frame, _("Recreate signing certificates"),
				variant::wx::insert_dcpomatic(
					_("The certificate chain that %s uses for signing DCPs and KDMs contains a small error\n"
					  "which will prevent DCPs from being validated correctly on some systems.  Do you want to re-create\n"
					  "the certificate chain for signing DCPs and KDMs?")
					),
				_("Do nothing"),
				Config::NAG_BAD_SIGNER_CHAIN_UTF8
				);
			return dialog.ShowModal() == wxID_OK;
		}
		case Config::BAD_SIGNER_VALIDITY_TOO_LONG:
		{
			if (config->nagged(Config::NAG_BAD_SIGNER_CHAIN_VALIDITY)) {
				return false;
			}
			RecreateChainDialog dialog(
				_frame, _("Recreate signing certificates"),
				variant::wx::insert_dcpomatic(
					_("The certificate chain that %s uses for signing DCPs and KDMs has a validity period\n"
					  "that is too long.  This will cause problems playing back DCPs on some systems.\n"
					  "Do you want to re-create the certificate chain for signing DCPs and KDMs?")
					),
				_("Do nothing"),
				Config::NAG_BAD_SIGNER_CHAIN_VALIDITY
				);
			return dialog.ShowModal() == wxID_OK;
		}
		case Config::BAD_SIGNER_INCONSISTENT:
		{
			RecreateChainDialog dialog(
				_frame, _("Recreate signing certificates"),
				wxString::Format(
					_("The certificate chain that %s uses for signing DCPs and KDMs is inconsistent and\n"
					  "cannot be used.  %s cannot start unless you re-create it.  Do you want to re-create\n"
					  "the certificate chain for signing DCPs and KDMs?"),
					variant::wx::dcpomatic(),
					variant::wx::dcpomatic()
					),
				variant::wx::insert_dcpomatic(_("Close %s"))
				);
			if (dialog.ShowModal() != wxID_OK) {
				exit (EXIT_FAILURE);
			}
			return true;
		}
		case Config::BAD_DECRYPTION_INCONSISTENT:
		{
			RecreateChainDialog dialog(
				_frame, _("Recreate KDM decryption chain"),
				wxString::Format(
					_("The certificate chain that %s uses for decrypting KDMs is inconsistent and\n"
					  "cannot be used.  %s cannot start unless you re-create it.  Do you want to re-create\n"
					  "the certificate chain for decrypting KDMs?  You may want to say \"No\" here and back up your\n"
					  "configuration before continuing."),
					variant::wx::dcpomatic(),
					variant::wx::dcpomatic()
					),
				variant::wx::insert_dcpomatic(_("Close %s"))
				);
			if (dialog.ShowModal() != wxID_OK) {
				exit (EXIT_FAILURE);
			}
			return true;
		}
		case Config::BAD_SIGNER_DN_QUALIFIER:
		{
			RecreateChainDialog dialog(
				_frame, _("Recreate signing certificates"),
				wxString::Format(
					_("The certificate chain that %s uses for signing DCPs and KDMs contains a small error\n"
					  "which will prevent DCPs from being validated correctly on some systems.  This error was caused\n"
					  "by a bug in %s which has now been fixed. Do you want to re-create the certificate chain\n"
					  "for signing DCPs and KDMs?"),
					variant::wx::dcpomatic(),
					variant::wx::dcpomatic()
					),
				_("Do nothing"),
				Config::NAG_BAD_SIGNER_DN_QUALIFIER
				);
			return dialog.ShowModal() == wxID_OK;
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
