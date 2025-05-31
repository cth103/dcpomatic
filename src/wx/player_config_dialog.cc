/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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


/** @file src/player_config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic Player configuration.
 */


#include "check_box.h"
#include "dcpomatic_choice.h"
#include "dir_picker_ctrl.h"
#include "editable_list.h"
#include "email_dialog.h"
#include "file_picker_ctrl.h"
#include "filter_dialog.h"
#include "general_preferences_page.h"
#include "keys_preferences_page.h"
#include "locations_preferences_page.h"
#include "make_chain_dialog.h"
#include "nag_dialog.h"
#include "name_format_editor.h"
#include "preferences_page.h"
#include "server_dialog.h"
#include "sound_preferences_page.h"
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcp_content_type.h"
#include "lib/exceptions.h"
#include "lib/filter.h"
#include "lib/log.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/locale_convert.h>
#include <dcp/scope_guard.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using std::function;
using std::list;
using std::make_pair;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;
using namespace dcpomatic;


class PlayerGeneralPage : public preferences::GeneralPage
{
public:
	PlayerGeneralPage(wxSize panel_size, int border)
		: GeneralPage(panel_size, border)
	{}

private:
	void setup() override
	{
		auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls(table, r);
		add_update_controls(table, r);

		_enable_http_server = new CheckBox(_panel, _("Enable HTTP control interface on port"));
		table->Add(_enable_http_server, wxGBPosition(r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		_http_server_port = new wxSpinCtrl(_panel);
		table->Add(_http_server_port, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer(table, _panel, _("Start player as"), true, wxGBPosition(r, 0));
		_player_mode = new wxChoice(_panel, wxID_ANY);
		_player_mode->Append(_("window"));
		_player_mode->Append(_("full screen"));
		_player_mode->Append(_("full screen with separate advanced controls"));
		table->Add(_player_mode, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer(table, _panel, _("Dual-screen displays"), true, wxGBPosition(r, 0));
		_image_display = new wxChoice(_panel, wxID_ANY);
		_image_display->Append(_("Image on primary, controls on secondary"));
		_image_display->Append(_("Image on secondary, controls on primary"));
		table->Add(_image_display, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer(table, _panel, _("Video display mode"), true, wxGBPosition(r, 0));
		_video_display_mode = new wxChoice(_panel, wxID_ANY);
		_video_display_mode->Append(_("Simple (safer)"));
		_video_display_mode->Append(_("OpenGL (faster)"));
		table->Add(_video_display_mode, wxGBPosition(r, 1));
		++r;

		auto restart = add_label_to_sizer(table, _panel, variant::wx::insert_dcpomatic_player(_("(restart %s to change display mode)")), false, wxGBPosition(r, 0));
		wxFont font = restart->GetFont();
		font.SetStyle(wxFONTSTYLE_ITALIC);
		font.SetPointSize(font.GetPointSize() - 1);
		restart->SetFont(font);
		++r;

		_respect_kdm = new CheckBox(_panel, _("Respect KDM validity periods"));
		table->Add(_respect_kdm, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

		add_label_to_sizer(table, _panel, _("Debug log file"), true, wxGBPosition(r, 0));
		_debug_log_file = new FilePickerCtrl(_panel, _("Select debug log file"), char_to_wx("*"), false, true, "DebugLogPath");
		table->Add(_debug_log_file, wxGBPosition(r, 1));
		++r;

		_player_mode->Bind(wxEVT_CHOICE, bind(&PlayerGeneralPage::player_mode_changed, this));
		_image_display->Bind(wxEVT_CHOICE, bind(&PlayerGeneralPage::image_display_changed, this));
		_video_display_mode->Bind(wxEVT_CHOICE, bind(&PlayerGeneralPage::video_display_mode_changed, this));
		_respect_kdm->bind(&PlayerGeneralPage::respect_kdm_changed, this);
		_debug_log_file->Bind(wxEVT_FILEPICKER_CHANGED, bind(&PlayerGeneralPage::debug_log_file_changed, this));
		_enable_http_server->bind(&PlayerGeneralPage::enable_http_server_changed, this);
		_http_server_port->SetRange(1, 32767);
		_http_server_port->Bind(wxEVT_SPINCTRL, boost::bind(&PlayerGeneralPage::http_server_port_changed, this));

		setup_sensitivity();
	}

	void config_changed() override
	{
		GeneralPage::config_changed();

		auto config = Config::instance();

		switch (config->player_mode()) {
		case Config::PlayerMode::WINDOW:
			checked_set(_player_mode, 0);
			break;
		case Config::PlayerMode::FULL:
			checked_set(_player_mode, 1);
			break;
		case Config::PlayerMode::DUAL:
			checked_set(_player_mode, 2);
			break;
		}

		switch (config->video_view_type()) {
		case Config::VIDEO_VIEW_SIMPLE:
			checked_set(_video_display_mode, 0);
			break;
		case Config::VIDEO_VIEW_OPENGL:
			checked_set(_video_display_mode, 1);
			break;
		}

		checked_set(_image_display, config->image_display());
		checked_set(_respect_kdm, config->respect_kdm_validity_periods());
		if (config->player_debug_log_file()) {
			checked_set(_debug_log_file, *config->player_debug_log_file());
		}

		checked_set(_enable_http_server, config->enable_player_http_server());
		checked_set(_http_server_port, config->player_http_server_port());

		setup_sensitivity();
	}

private:
	void player_mode_changed()
	{
		switch (_player_mode->GetSelection()) {
		case 0:
			Config::instance()->set_player_mode(Config::PlayerMode::WINDOW);
			break;
		case 1:
			Config::instance()->set_player_mode(Config::PlayerMode::FULL);
			break;
		case 2:
			Config::instance()->set_player_mode(Config::PlayerMode::DUAL);
			break;
		}
	}

	void image_display_changed()
	{
		Config::instance()->set_image_display(_image_display->GetSelection());
	}

	void video_display_mode_changed()
	{
		if (_video_display_mode->GetSelection() == 0) {
			Config::instance()->set_video_view_type(Config::VIDEO_VIEW_SIMPLE);
		} else {
			Config::instance()->set_video_view_type(Config::VIDEO_VIEW_OPENGL);
		}
	}

	void respect_kdm_changed()
	{
		Config::instance()->set_respect_kdm_validity_periods(_respect_kdm->GetValue());
	}

	void debug_log_file_changed()
	{
		if (auto path = _debug_log_file->path()) {
			Config::instance()->set_player_debug_log_file(*path);
		}
	}

	void enable_http_server_changed()
	{
		Config::instance()->set_enable_player_http_server(_enable_http_server->get());
	}

	void http_server_port_changed()
	{
		Config::instance()->set_player_http_server_port(_http_server_port->GetValue());
	}

	void setup_sensitivity()
	{
		_http_server_port->Enable(_enable_http_server->get());
	}

	wxChoice* _player_mode = nullptr;
	wxChoice* _image_display = nullptr;
	wxChoice* _video_display_mode = nullptr;
	CheckBox* _respect_kdm = nullptr;
	FilePickerCtrl* _debug_log_file = nullptr;
	CheckBox* _enable_http_server = nullptr;
	wxSpinCtrl* _http_server_port = nullptr;
};


/** @class PlayerAdvancedPage
 *  @brief Advanced page of the preferences dialog for the player.
 */
class PlayerAdvancedPage : public preferences::Page
{
public:
	PlayerAdvancedPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Advanced");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("advanced"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void add_top_aligned_label_to_sizer(wxSizer* table, wxWindow* parent, wxString text)
	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		text += char_to_wx(":");
#endif
		auto m = new StaticText(parent, text);
		table->Add(m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		_crop_output = new CheckBox(_panel, _("Crop output to"));
		table->Add(_crop_output, 0, wxEXPAND);
		auto s = new wxBoxSizer(wxHORIZONTAL);
		_crop_output_ratio_preset = new Choice(_panel);
		_crop_output_ratio_custom = new wxTextCtrl(_panel, wxID_ANY);

		s->Add(_crop_output_ratio_preset, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		s->Add(_crop_output_ratio_custom, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		add_label_to_sizer(s, _panel, _(":1"), false, 0, wxALIGN_CENTER_VERTICAL);

		for (auto ratio: Ratio::all()) {
			_crop_output_ratio_preset->add_entry(ratio.image_nickname(), ratio.id());
		}
		_crop_output_ratio_preset->add_entry(_("Custom"), "custom");
		table->Add(s, 1, wxEXPAND);

		{
			add_top_aligned_label_to_sizer(table, _panel, _("Log"));
			auto t = new wxBoxSizer(wxVERTICAL);
			_log_general = new CheckBox(_panel, _("General"));
			t->Add(_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new CheckBox(_panel, _("Warnings"));
			t->Add(_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new CheckBox(_panel, _("Errors"));
			t->Add(_log_error, 1, wxEXPAND | wxALL);
			/// TRANSLATORS: translate the word "Timing" here; do not include the "Config|" prefix
			_log_timing = new CheckBox(_panel, S_("Config|Timing"));
			t->Add(_log_timing, 1, wxEXPAND | wxALL);
			_log_debug_video_view = new CheckBox(_panel, _("Debug: video view"));
			t->Add(_log_debug_video_view, 1, wxEXPAND | wxALL);
			_log_debug_player = new CheckBox(_panel, _("Debug: player"));
			t->Add(_log_debug_player, 1, wxEXPAND | wxALL);
			table->Add(t, 0, wxALL, 6);
		}

#ifdef DCPOMATIC_WINDOWS
		_win32_console = new CheckBox(_panel, _("Open console window"));
		table->Add(_win32_console, 1, wxEXPAND | wxALL);
		table->AddSpacer(0);
#endif

		_log_general->bind(&PlayerAdvancedPage::log_changed, this);
		_log_warning->bind(&PlayerAdvancedPage::log_changed, this);
		_log_error->bind(&PlayerAdvancedPage::log_changed, this);
		_log_timing->bind(&PlayerAdvancedPage::log_changed, this);
		_log_debug_video_view->bind(&PlayerAdvancedPage::log_changed, this);
		_log_debug_player->bind(&PlayerAdvancedPage::log_changed, this);
#ifdef DCPOMATIC_WINDOWS
		_win32_console->bind(&PlayerAdvancedPage::win32_console_changed, this);
#endif
		_crop_output->bind(&PlayerAdvancedPage::crop_output_changed, this);
		_crop_output_ratio_preset->bind(&PlayerAdvancedPage::crop_output_ratio_preset_changed, this);
		_crop_output_ratio_custom->Bind(wxEVT_TEXT, boost::bind(&PlayerAdvancedPage::crop_output_ratio_custom_changed, this));

		checked_set(_crop_output, Config::instance()->player_crop_output_ratio().has_value());
		set_crop_output_ratio_preset_from_config();
		set_crop_output_ratio_custom_from_config();
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set(_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set(_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set(_log_timing, config->log_types() & LogEntry::TYPE_TIMING);
		checked_set(_log_debug_video_view, config->log_types() & LogEntry::TYPE_DEBUG_VIDEO_VIEW);
		checked_set(_log_debug_player, config->log_types() & LogEntry::TYPE_DEBUG_PLAYER);
#ifdef DCPOMATIC_WINDOWS
		checked_set(_win32_console, config->win32_console());
#endif

		/* Don't update crop ratio stuff here as the controls are all interdependent
		 * and nobody else is going to change the values.
		 */
	}

	void set_crop_output_ratio_preset_from_config()
	{
		_ignore_crop_changes = true;
		dcp::ScopeGuard sg = [this]() { _ignore_crop_changes = false; };

		if (auto output_ratio = Config::instance()->player_crop_output_ratio()) {
			auto ratio = Ratio::from_ratio(*output_ratio);
			_crop_output_ratio_preset->set_by_data(ratio ? ratio->id() : "custom");
		} else {
			_crop_output_ratio_preset->set_by_data("185");
		}
	}

	void set_crop_output_ratio_custom_from_config()
	{
		_ignore_crop_changes = true;
		dcp::ScopeGuard sg = [this]() { _ignore_crop_changes = false; };

		if (auto output_ratio = Config::instance()->player_crop_output_ratio()) {
			auto ratio = Ratio::from_ratio(*output_ratio);
			_crop_output_ratio_custom->SetValue(wxString::Format(char_to_wx("%.2f"), ratio ? ratio->ratio() : *output_ratio));
		} else {
			_crop_output_ratio_custom->SetValue(_("185"));
		}
	}

	void log_changed()
	{
		int types = 0;
		if (_log_general->GetValue()) {
			types |= LogEntry::TYPE_GENERAL;
		}
		if (_log_warning->GetValue()) {
			types |= LogEntry::TYPE_WARNING;
		}
		if (_log_error->GetValue())  {
			types |= LogEntry::TYPE_ERROR;
		}
		if (_log_timing->GetValue()) {
			types |= LogEntry::TYPE_TIMING;
		}
		if (_log_debug_video_view->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_VIDEO_VIEW;
		}
		if (_log_debug_player->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_PLAYER;
		}
		Config::instance()->set_log_types(types);
	}

	void crop_output_changed()
	{
		if (_crop_output->get()) {
			Config::instance()->set_player_crop_output_ratio(1.85);
			set_crop_output_ratio_preset_from_config();
			set_crop_output_ratio_custom_from_config();
		} else {
			Config::instance()->unset_player_crop_output_ratio();
		}
		_crop_output_ratio_preset->Enable(_crop_output->get());
		_crop_output_ratio_custom->Enable(_crop_output->get());
	}

	void crop_output_ratio_preset_changed()
	{
		if (!_crop_output->get() || _ignore_crop_changes) {
			return;
		}

		optional<Ratio> ratio;

		if (auto data = _crop_output_ratio_preset->get_data()) {
			ratio = Ratio::from_id_if_exists(*data);
		}

		if (ratio) {
			Config::instance()->set_player_crop_output_ratio(ratio->ratio());
		} else {
			Config::instance()->set_player_crop_output_ratio(locale_convert<float>(wx_to_std(_crop_output_ratio_custom->GetValue())));
		}

		set_crop_output_ratio_custom_from_config();
	}

	void crop_output_ratio_custom_changed()
	{
		if (!_crop_output->get() || _ignore_crop_changes) {
			return;
		}

		Config::instance()->set_player_crop_output_ratio(locale_convert<float>(wx_to_std(_crop_output_ratio_custom->GetValue())));
		set_crop_output_ratio_preset_from_config();
	}

#ifdef DCPOMATIC_WINDOWS
	void win32_console_changed()
	{
		Config::instance()->set_win32_console(_win32_console->GetValue());
	}
#endif

	CheckBox* _crop_output = nullptr;
	Choice* _crop_output_ratio_preset = nullptr;
	wxTextCtrl* _crop_output_ratio_custom = nullptr;
	bool _ignore_crop_changes = false;
	CheckBox* _log_general = nullptr;
	CheckBox* _log_warning = nullptr;
	CheckBox* _log_error = nullptr;
	CheckBox* _log_timing = nullptr;
	CheckBox* _log_debug_video_view = nullptr;
	CheckBox* _log_debug_player = nullptr;
#ifdef DCPOMATIC_WINDOWS
	CheckBox* _win32_console = nullptr;
#endif
};


wxPreferencesEditor*
create_player_config_dialog()
{
	auto e = new wxPreferencesEditor(variant::wx::insert_dcpomatic_player(_("%s Preferences")));

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	auto ps = wxSize(520, -1);
	int const border = 16;
#else
	auto ps = wxSize(-1, -1);
	int const border = 8;
#endif

	e->AddPage(new PlayerGeneralPage(wxSize(-1, 500), border));
	e->AddPage(new preferences::SoundPage(ps, border));
	e->AddPage(new preferences::LocationsPage(ps, border));
	e->AddPage(new preferences::KeysPage(ps, border));
	e->AddPage(new PlayerAdvancedPage(ps, border));
	return e;
}
