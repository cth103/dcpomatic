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


/** @file src/full_config_dialog.cc
 *  @brief A dialogue to edit all DCP-o-matic configuration.
 */


#include "check_box.h"
#include "config_move_dialog.h"
#include "dcpomatic_button.h"
#include "dcpomatic_choice.h"
#include "dir_picker_ctrl.h"
#include "editable_list.h"
#include "email_dialog.h"
#include "email_preferences_page.h"
#include "file_picker_ctrl.h"
#include "filter_dialog.h"
#include "full_config_dialog.h"
#include "general_preferences_page.h"
#include "kdm_choice.h"
#include "kdm_email_preferences_page.h"
#include "keys_preferences_page.h"
#include "language_tag_widget.h"
#include "make_chain_dialog.h"
#include "nag_dialog.h"
#include "name_format_editor.h"
#include "password_entry.h"
#include "preferences_page.h"
#include "region_subtag_widget.h"
#include "send_test_email_dialog.h"
#include "server_dialog.h"
#include "sound_preferences_page.h"
#include "static_text.h"
#ifdef DCPOMATIC_GROK
#include "grok/gpu_config_panel.h"
#endif
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/email.h"
#include "lib/exceptions.h"
#include "lib/filter.h"
#include "lib/log.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/locale_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>
#include <iostream>


using std::cout;
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


class FullGeneralPage : public preferences::GeneralPage
{
public:
	FullGeneralPage(wxSize panel_size, int border)
		: GeneralPage(panel_size, border)
	{}

private:
	void setup() override
	{
		auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls(table, r);

		add_label_to_sizer(table, _panel, variant::wx::insert_dcpomatic(_("Number of threads %s should use")), true, wxGBPosition(r, 0));
		_master_encoding_threads = new wxSpinCtrl(_panel);
		table->Add(_master_encoding_threads, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer(table, _panel, variant::wx::insert_dcpomatic_encode_server(_("Number of threads %s should use")), true, wxGBPosition(r, 0));
		_server_encoding_threads = new wxSpinCtrl(_panel);
		table->Add(_server_encoding_threads, wxGBPosition(r, 1));
		++r;

		add_config_file_controls(table, r);

		add_label_to_sizer(table, _panel, _("Default \"add file\" location"), true, wxGBPosition(r, 0));
		_default_add_file_location = new Choice(_panel);
		table->Add(_default_add_file_location, wxGBPosition(r, 1));
		++r;

		_relative_paths = new CheckBox(_panel, _("Write relative content paths"));
		table->Add(_relative_paths, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128 = new CheckBox(_panel, _("Find integrated loudness, true peak and loudness range when analysing audio"));
		table->Add(_analyse_ebur128, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;
#endif

		_automatic_audio_analysis = new CheckBox(_panel, _("Automatically analyse content audio"));
		table->Add(_automatic_audio_analysis, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

		add_update_controls(table, r);

		_default_add_file_location->add_entry(_("Same place as last time"));
		_default_add_file_location->add_entry(_("Same place as project"));
		_default_add_file_location->bind(&FullGeneralPage::default_add_file_location_changed, this);

		_master_encoding_threads->SetRange(1, 128);
		_master_encoding_threads->Bind(wxEVT_SPINCTRL, boost::bind(&FullGeneralPage::master_encoding_threads_changed, this));
		_server_encoding_threads->SetRange(1, 128);
		_server_encoding_threads->Bind(wxEVT_SPINCTRL, boost::bind(&FullGeneralPage::server_encoding_threads_changed, this));

		_relative_paths->bind(&FullGeneralPage::relative_paths_changed, this);
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128->bind(&FullGeneralPage::analyse_ebur128_changed, this);
#endif
		_automatic_audio_analysis->bind(&FullGeneralPage::automatic_audio_analysis_changed, this);
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_master_encoding_threads, config->master_encoding_threads());
		checked_set(_server_encoding_threads, config->server_encoding_threads());
		checked_set(_relative_paths, config->relative_paths());
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		checked_set(_analyse_ebur128, config->analyse_ebur128());
#endif
		checked_set(_automatic_audio_analysis, config->automatic_audio_analysis());
		checked_set(_default_add_file_location, config->default_add_file_location() == Config::DefaultAddFileLocation::SAME_AS_LAST_TIME ? 0 : 1);

		GeneralPage::config_changed();
	}

	void relative_paths_changed()
	{
		Config::instance()->set_relative_paths(_relative_paths->get());
	}

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	void analyse_ebur128_changed()
	{
		Config::instance()->set_analyse_ebur128(_analyse_ebur128->GetValue());
	}
#endif

	void automatic_audio_analysis_changed()
	{
		Config::instance()->set_automatic_audio_analysis(_automatic_audio_analysis->GetValue());
	}

	void master_encoding_threads_changed()
	{
		Config::instance()->set_master_encoding_threads(_master_encoding_threads->GetValue());
	}

	void server_encoding_threads_changed()
	{
		Config::instance()->set_server_encoding_threads(_server_encoding_threads->GetValue());
	}

	void default_add_file_location_changed()
	{
		Config::instance()->set_default_add_file_location(
			_default_add_file_location->get().get_value_or(0) == 0 ? Config::DefaultAddFileLocation::SAME_AS_LAST_TIME : Config::DefaultAddFileLocation::SAME_AS_PROJECT
			);
	}

	Choice* _default_add_file_location;
	wxSpinCtrl* _master_encoding_threads;
	wxSpinCtrl* _server_encoding_threads;
	CheckBox* _relative_paths;
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	CheckBox* _analyse_ebur128;
#endif
	CheckBox* _automatic_audio_analysis;
};


class DefaultsPage : public preferences::Page
{
public:
	DefaultsPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Defaults");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("defaults"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer(table, _panel, _("Default duration of still images"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_still_length = new wxSpinCtrl(_panel);
			s->Add(_still_length);
			add_label_to_sizer(s, _panel, _("s"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			table->Add(s, 1);
		}

		add_label_to_sizer(table, _panel, _("Default directory for new films"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_directory = new DirPickerCtrl(_panel);
#else
		_directory = new wxDirPickerCtrl(_panel, wxDD_DIR_MUST_EXIST);
#endif
		table->Add(_directory, 1, wxEXPAND);

		{
			add_label_to_sizer(table, _panel, _("Default audio delay"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_audio_delay = new wxSpinCtrl(_panel);
			s->Add(_audio_delay);
			add_label_to_sizer(s, _panel, _("ms"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			table->Add(s, 1);
		}

		_enable_audio_language = new CheckBox(_panel, _("Default audio language"));
		table->Add(_enable_audio_language, 1, wxEXPAND | wxALIGN_CENTRE_VERTICAL);
		_audio_language = new LanguageTagWidget(_panel, _("Default audio language to use for new DCPs"), Config::instance()->default_audio_language(), char_to_wx("cmnr-Hant-"));
		table->Add(_audio_language->sizer());

		add_label_to_sizer(table, _panel, _("Default KDM directory"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_kdm_directory = new DirPickerCtrl(_panel);
#else
		_kdm_directory = new wxDirPickerCtrl(_panel, wxDD_DIR_MUST_EXIST);
#endif
		table->Add(_kdm_directory, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("Default KDM type"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_kdm_type = new KDMChoice(_panel);
		table->Add(_kdm_type, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("Default KDM duration"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_kdm_duration = new wxSpinCtrl(_panel);
		_kdm_duration_unit = new wxChoice(_panel, wxID_ANY);
		auto kdm_duration_sizer = new wxBoxSizer(wxHORIZONTAL);
		kdm_duration_sizer->Add(_kdm_duration, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		kdm_duration_sizer->Add(_kdm_duration_unit, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		table->Add(kdm_duration_sizer, 1, wxEXPAND);

		table->Add(_use_isdcf_name_by_default = new CheckBox(_panel, _("Use ISDCF name by default")), 0, wxALIGN_CENTRE_VERTICAL);

		_still_length->SetRange(1, 3600);
		_still_length->Bind(wxEVT_SPINCTRL, boost::bind(&DefaultsPage::still_length_changed, this));

		_directory->Bind(wxEVT_DIRPICKER_CHANGED, boost::bind(&DefaultsPage::directory_changed, this));
		_kdm_directory->Bind(wxEVT_DIRPICKER_CHANGED, boost::bind(&DefaultsPage::kdm_directory_changed, this));
		_kdm_type->Bind(wxEVT_CHOICE, boost::bind(&DefaultsPage::kdm_type_changed, this));
		_kdm_duration_unit->Append(_("days"));
		_kdm_duration_unit->Append(_("weeks"));
		_kdm_duration_unit->Append(_("months"));
		_kdm_duration_unit->Append(_("years"));

		_kdm_duration->Bind(wxEVT_SPINCTRL, boost::bind(&DefaultsPage::kdm_duration_changed, this));
		_kdm_duration_unit->Bind(wxEVT_CHOICE, boost::bind(&DefaultsPage::kdm_duration_changed, this));

		_use_isdcf_name_by_default->bind(&DefaultsPage::use_isdcf_name_by_default_changed, this);

		_audio_delay->SetRange(-1000, 1000);
		_audio_delay->Bind(wxEVT_SPINCTRL, boost::bind(&DefaultsPage::audio_delay_changed, this));

		_enable_audio_language->bind(&DefaultsPage::enable_audio_language_toggled, this);
		_audio_language->Changed.connect(boost::bind(&DefaultsPage::audio_language_changed, this));
	}

	void config_changed() override
	{
		auto config = Config::instance();
		checked_set(_still_length, config->default_still_length());
		_directory->SetPath(std_to_wx(config->default_directory_or(wx_to_std(wxStandardPaths::Get().GetDocumentsDir())).string()));
		_kdm_directory->SetPath(std_to_wx(config->default_kdm_directory_or(wx_to_std(wxStandardPaths::Get().GetDocumentsDir())).string()));
		_kdm_type->set(config->default_kdm_type());
		checked_set(_use_isdcf_name_by_default, config->use_isdcf_name_by_default());
		checked_set(_audio_delay, config->default_audio_delay());
		auto dal = config->default_audio_language();
		checked_set(_enable_audio_language, static_cast<bool>(dal));
		checked_set(_audio_language, dal ? dal : boost::none);

		checked_set(_kdm_duration, config->default_kdm_duration().duration);
		switch (config->default_kdm_duration().unit) {
			case RoughDuration::Unit::DAYS:
				_kdm_duration->SetRange(1, 365);
				checked_set(_kdm_duration_unit, 0);
				break;
			case RoughDuration::Unit::WEEKS:
				_kdm_duration->SetRange(1, 52);
				checked_set(_kdm_duration_unit, 1);
				break;
			case RoughDuration::Unit::MONTHS:
				_kdm_duration->SetRange(1, 12);
				checked_set(_kdm_duration_unit, 2);
				break;
			case RoughDuration::Unit::YEARS:
				_kdm_duration->SetRange(1, 40);
				checked_set(_kdm_duration_unit, 3);
				break;
		}

		setup_sensitivity();
	}

	void kdm_duration_changed()
	{
		auto config = Config::instance();
		auto duration = _kdm_duration->GetValue();
		RoughDuration::Unit unit = RoughDuration::Unit::DAYS;
		switch (_kdm_duration_unit->GetSelection()) {
		case 0:
			unit = RoughDuration::Unit::DAYS;
			break;
		case 1:
			unit = RoughDuration::Unit::WEEKS;
			break;
		case 2:
			unit = RoughDuration::Unit::MONTHS;
			break;
		case 3:
			unit = RoughDuration::Unit::YEARS;
			break;
		}
		config->set_default_kdm_duration(RoughDuration(duration, unit));
	}

	void audio_delay_changed()
	{
		Config::instance()->set_default_audio_delay(_audio_delay->GetValue());
	}

	void directory_changed()
	{
		Config::instance()->set_default_directory(wx_to_std(_directory->GetPath()));
	}

	void kdm_directory_changed()
	{
		Config::instance()->set_default_kdm_directory(wx_to_std(_kdm_directory->GetPath()));
	}

	void kdm_type_changed()
	{
		Config::instance()->set_default_kdm_type(_kdm_type->get());
	}

	void use_isdcf_name_by_default_changed()
	{
		Config::instance()->set_use_isdcf_name_by_default(_use_isdcf_name_by_default->GetValue());
	}

	void still_length_changed()
	{
		Config::instance()->set_default_still_length(_still_length->GetValue());
	}

	void enable_audio_language_toggled()
	{
		setup_sensitivity();
		audio_language_changed();
	}

	void audio_language_changed()
	{
		if (_enable_audio_language->get()) {
			Config::instance()->set_default_audio_language(_audio_language->get().get_value_or(dcp::LanguageTag("en-US")));
		} else {
			Config::instance()->unset_default_audio_language();
		}
	}

	void setup_sensitivity()
	{
		_audio_language->enable(_enable_audio_language->get());
	}

	wxSpinCtrl* _audio_delay;
	wxSpinCtrl* _still_length;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _directory;
	DirPickerCtrl* _kdm_directory;
#else
	wxDirPickerCtrl* _directory;
	wxDirPickerCtrl* _kdm_directory;
#endif
	KDMChoice* _kdm_type;
	wxSpinCtrl* _kdm_duration;
	wxChoice* _kdm_duration_unit;
	CheckBox* _use_isdcf_name_by_default;
	CheckBox* _enable_audio_language;
	LanguageTagWidget* _audio_language;
};


class EncodingServersPage : public preferences::Page
{
public:
	EncodingServersPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Servers");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("servers"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		_use_any_servers = new CheckBox(_panel, _("Search network for servers"));
		_panel->GetSizer()->Add(_use_any_servers, 0, wxALL, _border);

		vector<EditableListColumn> columns;
		columns.push_back(EditableListColumn(_("IP address / host name")));
		_servers_list = new EditableList<string, ServerDialog>(
			_panel,
			columns,
			boost::bind(&Config::servers, Config::instance()),
			boost::bind(&Config::set_servers, Config::instance(), _1),
			boost::bind(&EncodingServersPage::server_column, this, _1),
			EditableListTitle::INVISIBLE,
			EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
			);

		_panel->GetSizer()->Add(_servers_list, 1, wxEXPAND | wxALL, _border);

		_use_any_servers->bind(&EncodingServersPage::use_any_servers_changed, this);
	}

	void config_changed() override
	{
		checked_set(_use_any_servers, Config::instance()->use_any_servers());
		_servers_list->refresh();
	}

	void use_any_servers_changed()
	{
		Config::instance()->set_use_any_servers(_use_any_servers->GetValue());
	}

	string server_column(string s)
	{
		return s;
	}

	CheckBox* _use_any_servers;
	EditableList<string, ServerDialog>* _servers_list;
};


class TMSPage : public preferences::Page
{
public:
	TMSPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("TMS");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("tms"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		_upload = new CheckBox(_panel, _("Upload DCP to TMS after creation"));
		_panel->GetSizer()->Add(_upload, 0, wxALL | wxEXPAND, _border);

		wxFlexGridSizer* table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer(table, _panel, _("Protocol"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_tms_protocol = new wxChoice(_panel, wxID_ANY);
		table->Add(_tms_protocol, 1, wxEXPAND);

		_tms_passive = new CheckBox(_panel, _("Passive mode"));
		table->Add(_tms_passive, 1, wxEXPAND);
		table->AddSpacer(0);

		add_label_to_sizer(table, _panel, _("IP address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_tms_ip = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_tms_ip, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("Target path"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_tms_path = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_tms_path, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("User name"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_tms_user = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_tms_user, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("Password"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_tms_password = new PasswordEntry(_panel);
		table->Add(_tms_password->get_panel(), 1, wxEXPAND);

		_tms_protocol->Append(_("SCP (for AAM and Doremi)"));
		_tms_protocol->Append(_("FTP (for Dolby)"));

		_upload->bind(&TMSPage::upload_changed, this);
		_tms_protocol->Bind(wxEVT_CHOICE, boost::bind(&TMSPage::tms_protocol_changed, this));
		_tms_passive->bind(&TMSPage::tms_passive_changed, this);

		_tms_ip->Bind(wxEVT_TEXT, boost::bind(&TMSPage::tms_ip_changed, this));
		_tms_path->Bind(wxEVT_TEXT, boost::bind(&TMSPage::tms_path_changed, this));
		_tms_user->Bind(wxEVT_TEXT, boost::bind(&TMSPage::tms_user_changed, this));
		_tms_password->Changed.connect(boost::bind(&TMSPage::tms_password_changed, this));
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_upload, config->upload_after_make_dcp());
		checked_set(_tms_protocol, static_cast<int>(config->tms_protocol()));
		checked_set(_tms_passive, config->tms_protocol() == FileTransferProtocol::FTP && config->tms_passive());
		checked_set(_tms_ip, config->tms_ip());
		checked_set(_tms_path, config->tms_path());
		checked_set(_tms_user, config->tms_user());
		checked_set(_tms_password, config->tms_password());

		_tms_passive->Enable(config->tms_protocol() == FileTransferProtocol::FTP);
	}

	void upload_changed()
	{
		Config::instance()->set_upload_after_make_dcp(_upload->GetValue());
	}

	void tms_protocol_changed()
	{
		Config::instance()->set_tms_protocol(static_cast<FileTransferProtocol>(_tms_protocol->GetSelection()));
	}

	void tms_passive_changed()
	{
		Config::instance()->set_tms_passive(_tms_passive->get());
	}

	void tms_ip_changed()
	{
		Config::instance()->set_tms_ip(wx_to_std(_tms_ip->GetValue()));
	}

	void tms_path_changed()
	{
		Config::instance()->set_tms_path(wx_to_std(_tms_path->GetValue()));
	}

	void tms_user_changed()
	{
		Config::instance()->set_tms_user(wx_to_std(_tms_user->GetValue()));
	}

	void tms_password_changed()
	{
		Config::instance()->set_tms_password(_tms_password->get());
	}

	CheckBox* _upload;
	CheckBox* _tms_passive;
	wxChoice* _tms_protocol;
	wxTextCtrl* _tms_ip;
	wxTextCtrl* _tms_path;
	wxTextCtrl* _tms_user;
	PasswordEntry* _tms_password;
};


class NotificationsPage : public preferences::Page
{
public:
	NotificationsPage(wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: Page(wxSize(panel_size.GetWidth(), 128), border)
#else
		: Page(panel_size, border)
#endif
	{}

	wxString GetName() const override
	{
		return _("Notifications");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("notifications"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 0, wxEXPAND | wxALL, _border);

		_enable_message_box = new CheckBox(_panel, _("Message box"));
		table->Add(_enable_message_box, 1, wxEXPAND | wxALL);
		table->AddSpacer(0);

		_enable_email = new CheckBox(_panel, _("Email"));
		table->Add(_enable_email, 1, wxEXPAND | wxALL);
		table->AddSpacer(0);

		add_label_to_sizer(table, _panel, _("Subject"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_subject = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_subject, 1, wxEXPAND | wxALL);

		add_label_to_sizer(table, _panel, _("From address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_from = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_from, 1, wxEXPAND | wxALL);

		add_label_to_sizer(table, _panel, _("To address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_to = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_to, 1, wxEXPAND | wxALL);

		vector<EditableListColumn> columns;
		columns.push_back(EditableListColumn(_("Address")));
		add_label_to_sizer(table, _panel, _("CC addresses"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_cc = new EditableList<string, EmailDialog>(
			_panel,
			columns,
			bind(&Config::notification_cc, Config::instance()),
			bind(&Config::set_notification_cc, Config::instance(), _1),
			[] (string s, int) {
				return s;
			},
			EditableListTitle::VISIBLE,
			EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
			);
		table->Add(_cc, 1, wxEXPAND | wxALL);

		add_label_to_sizer(table, _panel, _("BCC address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_bcc = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_bcc, 1, wxEXPAND | wxALL);

		_email = new wxTextCtrl(_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add(_email, 0, wxEXPAND | wxALL, _border);

		_reset_email = new Button(_panel, _("Reset to default subject and text"));
		_panel->GetSizer()->Add(_reset_email, 0, wxEXPAND | wxALL, _border);

		_cc->layout();

		_enable_message_box->bind(&NotificationsPage::type_changed, this, _enable_message_box, Config::MESSAGE_BOX);
		_enable_email->bind(&NotificationsPage::type_changed, this, _enable_email, Config::EMAIL);

		_subject->Bind(wxEVT_TEXT, boost::bind(&NotificationsPage::notification_subject_changed, this));
		_from->Bind(wxEVT_TEXT, boost::bind(&NotificationsPage::notification_from_changed, this));
		_to->Bind(wxEVT_TEXT, boost::bind(&NotificationsPage::notification_to_changed, this));
		_bcc->Bind(wxEVT_TEXT, boost::bind(&NotificationsPage::notification_bcc_changed, this));
		_email->Bind(wxEVT_TEXT, boost::bind(&NotificationsPage::notification_email_changed, this));
		_reset_email->Bind(wxEVT_BUTTON, boost::bind(&NotificationsPage::reset_email, this));

		setup_sensitivity();
	}

	void setup_sensitivity()
	{
		bool const s = _enable_email->GetValue();
		_subject->Enable(s);
		_from->Enable(s);
		_to->Enable(s);
		_cc->Enable(s);
		_bcc->Enable(s);
		_email->Enable(s);
		_reset_email->Enable(s);
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_enable_message_box, config->notification(Config::MESSAGE_BOX));
		checked_set(_enable_email, config->notification(Config::EMAIL));
		checked_set(_subject, config->notification_subject());
		checked_set(_from, config->notification_from());
		checked_set(_to, config->notification_to());
		checked_set(_bcc, config->notification_bcc());
		checked_set(_email, Config::instance()->notification_email());

		setup_sensitivity();
	}

	void notification_subject_changed()
	{
		Config::instance()->set_notification_subject(wx_to_std(_subject->GetValue()));
	}

	void notification_from_changed()
	{
		Config::instance()->set_notification_from(wx_to_std(_from->GetValue()));
	}

	void notification_to_changed()
	{
		Config::instance()->set_notification_to(wx_to_std(_to->GetValue()));
	}

	void notification_bcc_changed()
	{
		Config::instance()->set_notification_bcc(wx_to_std(_bcc->GetValue()));
	}

	void notification_email_changed()
	{
		if (_email->GetValue().IsEmpty()) {
			/* Sometimes we get sent an erroneous notification that the email
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_notification_email(wx_to_std(_email->GetValue()));
	}

	void reset_email()
	{
		Config::instance()->reset_notification_email();
		checked_set(_email, Config::instance()->notification_email());
	}

	void type_changed(CheckBox* b, Config::Notification n)
	{
		Config::instance()->set_notification(n, b->GetValue());
		setup_sensitivity();
	}

	CheckBox* _enable_message_box;
	CheckBox* _enable_email;

	wxTextCtrl* _subject;
	wxTextCtrl* _from;
	wxTextCtrl* _to;
	EditableList<string, EmailDialog>* _cc;
	wxTextCtrl* _bcc;
	wxTextCtrl* _email;
	wxButton* _reset_email;
};


class CoverSheetPage : public preferences::Page
{
public:

	CoverSheetPage(wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: Page(wxSize(panel_size.GetWidth(), 128), border)
#else
		: Page(panel_size, border)
#endif
	{}

	wxString GetName() const override
	{
		return _("Cover Sheet");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("cover_sheet"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		_cover_sheet = new wxTextCtrl(_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add(_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_reset_cover_sheet = new Button(_panel, _("Reset to default text"));
		_panel->GetSizer()->Add(_reset_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_cover_sheet->Bind(wxEVT_TEXT, boost::bind(&CoverSheetPage::cover_sheet_changed, this));
		_reset_cover_sheet->Bind(wxEVT_BUTTON, boost::bind(&CoverSheetPage::reset_cover_sheet, this));
	}

	void config_changed() override
	{
		checked_set(_cover_sheet, Config::instance()->cover_sheet());
	}

	void cover_sheet_changed()
	{
		if (_cover_sheet->GetValue().IsEmpty()) {
			/* Sometimes we get sent an erroneous notification that the cover sheet
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_cover_sheet(wx_to_std(_cover_sheet->GetValue()));
	}

	void reset_cover_sheet()
	{
		Config::instance()->reset_cover_sheet();
		checked_set(_cover_sheet, Config::instance()->cover_sheet());
	}

	wxTextCtrl* _cover_sheet;
	wxButton* _reset_cover_sheet;
};


class IdentifiersPage : public preferences::Page
{
public:
	IdentifiersPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Identifiers");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("identifiers"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);

		add_label_to_sizer(table, _panel, _("Issuer"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_issuer = new wxTextCtrl(_panel, wxID_ANY);
		_issuer->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's XML files as the <Issuer>.  If it is blank, a default value mentioning %s will be used.")
				));
		table->Add(_issuer, 1, wxALL | wxEXPAND);

		add_label_to_sizer(table, _panel, _("Creator"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_creator = new wxTextCtrl(_panel, wxID_ANY);
		_creator->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's XML files as the <Creator>.  If it is blank, a default value mentioning %s will be used.")
				));
		table->Add(_creator, 1, wxALL | wxEXPAND);

		add_label_to_sizer(table, _panel, _("Company name"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_company_name = new wxTextCtrl(_panel, wxID_ANY);
		_company_name->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's MXF files as the 'company name'.  If it is blank, a default value mentioning libdcp (an internal %s library) will be used.")
				));
		table->Add(_company_name, 1, wxALL | wxEXPAND);

		add_label_to_sizer(table, _panel, _("Product name"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_product_name = new wxTextCtrl(_panel, wxID_ANY);
		_product_name->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's MXF files as the 'product name'.  If it is blank, a default value mentioning libdcp (an internal %s library) will be used.")
				));
		table->Add(_product_name, 1, wxALL | wxEXPAND);

		add_label_to_sizer(table, _panel, _("Product version"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_product_version = new wxTextCtrl(_panel, wxID_ANY);
		_product_version->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's MXF files as the 'product version'.  If it is blank, a default value mentioning libdcp (an internal %s library) will be used.")
				));
		table->Add(_product_version, 1, wxALL | wxEXPAND);

		add_label_to_sizer(table, _panel, _("JPEG2000 comment"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_j2k_comment = new wxTextCtrl(_panel, wxID_ANY);
		_j2k_comment->SetToolTip(
			variant::wx::insert_dcpomatic(
				_("This will be written to the DCP's JPEG2000 data as a comment.  If it is blank, a default value mentioning libdcp (an internal %s library) will be used.")
				));
		table->Add(_j2k_comment, 1, wxALL | wxEXPAND);

		_panel->GetSizer()->Add(table, 0, wxEXPAND | wxALL, _border);

		_issuer->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::issuer_changed, this));
		_creator->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::creator_changed, this));
		_company_name->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::company_name_changed, this));
		_product_name->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::product_name_changed, this));
		_product_version->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::product_version_changed, this));
		_j2k_comment->Bind(wxEVT_TEXT, boost::bind(&IdentifiersPage::j2k_comment_changed, this));
	}

	void config_changed() override
	{
		auto config = Config::instance();
		checked_set(_issuer, config->dcp_issuer());
		checked_set(_creator, config->dcp_creator());
		checked_set(_company_name, config->dcp_company_name());
		checked_set(_product_name, config->dcp_product_name());
		checked_set(_product_version, config->dcp_product_version());
		checked_set(_j2k_comment, config->dcp_j2k_comment());
	}

	void issuer_changed()
	{
		Config::instance()->set_dcp_issuer(wx_to_std(_issuer->GetValue()));
	}

	void creator_changed()
	{
		Config::instance()->set_dcp_creator(wx_to_std(_creator->GetValue()));
	}

	void company_name_changed()
	{
		Config::instance()->set_dcp_company_name(wx_to_std(_company_name->GetValue()));
	}

	void product_name_changed()
	{
		Config::instance()->set_dcp_product_name(wx_to_std(_product_name->GetValue()));
	}

	void product_version_changed()
	{
		Config::instance()->set_dcp_product_version(wx_to_std(_product_version->GetValue()));
	}

	void j2k_comment_changed()
	{
		Config::instance()->set_dcp_j2k_comment(wx_to_std(_j2k_comment->GetValue()));
	}

	wxTextCtrl* _issuer;
	wxTextCtrl* _creator;
	wxTextCtrl* _company_name;
	wxTextCtrl* _product_name;
	wxTextCtrl* _product_version;
	wxTextCtrl* _j2k_comment;
};


class NonStandardPage : public preferences::Page
{
public:
	NonStandardPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Non-standard");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("non_standard"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer(table, _panel, _("Maximum JPEG2000 bit rate"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_maximum_j2k_video_bit_rate = new wxSpinCtrl(_panel);
			s->Add(_maximum_j2k_video_bit_rate, 1);
			add_label_to_sizer(s, _panel, _("Mbit/s"), false, 0, wxLEFT | wxALIGN_CENTRE_VERTICAL);
			table->Add(s, 1);
		}

		{
			add_label_to_sizer(table, _panel, _("Maximum MPEG2 bit rate"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_maximum_mpeg2_video_bit_rate = new wxSpinCtrl(_panel);
			s->Add(_maximum_mpeg2_video_bit_rate, 1);
			add_label_to_sizer(s, _panel, _("Mbit/s"), false, 0, wxLEFT | wxALIGN_CENTRE_VERTICAL);
			table->Add(s, 1);
		}

		auto checkbox = [this, table](wxString name, CheckBox*& variable) {
			variable = new CheckBox(_panel, name);
			table->Add(variable, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
			table->AddSpacer(0);
		};

		checkbox(_("Allow any DCP frame rate"),  _allow_any_dcp_frame_rate);

		_allow_any_container = new CheckBox(_panel, _("Allow full-frame and non-standard container ratios"));
		table->Add(_allow_any_container, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
		table->AddSpacer(0);

		checkbox(_("Allow creation of DCPs with 96kHz audio"), _allow_96khz_audio);
		checkbox(_("Allow mapping to all audio channels"), _use_all_audio_channels);
		checkbox(_("Allow use of SMPTE Bv2.0"), _allow_smpte_bv20);

		{
			add_label_to_sizer(table, _panel, _("ISDCF name part length"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_isdcf_name_part_length = new wxSpinCtrl(_panel);
			s->Add(_isdcf_name_part_length, 1);
			table->Add(s, 1);
		}

		_maximum_j2k_video_bit_rate->SetRange(1, 1000);
		_maximum_j2k_video_bit_rate->Bind(wxEVT_SPINCTRL, boost::bind(&NonStandardPage::maximum_j2k_video_bit_rate_changed, this));
		_maximum_mpeg2_video_bit_rate->SetRange(1, 100);
		_maximum_mpeg2_video_bit_rate->Bind(wxEVT_SPINCTRL, boost::bind(&NonStandardPage::maximum_mpeg2_video_bit_rate_changed, this));
		_allow_any_dcp_frame_rate->bind(&NonStandardPage::allow_any_dcp_frame_rate_changed, this);
		_allow_any_container->bind(&NonStandardPage::allow_any_container_changed, this);
		_allow_96khz_audio->bind(&NonStandardPage::allow_96khz_audio_changed, this);
		_use_all_audio_channels->bind(&NonStandardPage::use_all_channels_changed, this);
		_allow_smpte_bv20->bind(&NonStandardPage::allow_smpte_bv20_changed, this);
		_isdcf_name_part_length->SetRange(1, 256);
		_isdcf_name_part_length->Bind(wxEVT_SPINCTRL, boost::bind(&NonStandardPage::isdcf_name_part_length_changed, this));
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_maximum_j2k_video_bit_rate, config->maximum_video_bit_rate(VideoEncoding::JPEG2000) / 1000000);
		checked_set(_maximum_mpeg2_video_bit_rate, config->maximum_video_bit_rate(VideoEncoding::MPEG2) / 1000000);
		checked_set(_allow_any_dcp_frame_rate, config->allow_any_dcp_frame_rate());
		checked_set(_allow_any_container, config->allow_any_container());
		checked_set(_allow_96khz_audio, config->allow_96khz_audio());
		checked_set(_use_all_audio_channels, config->use_all_audio_channels());
		checked_set(_allow_smpte_bv20, config->allow_smpte_bv20());
		checked_set(_isdcf_name_part_length, config->isdcf_name_part_length());
	}

	void maximum_j2k_video_bit_rate_changed()
	{
		Config::instance()->set_maximum_video_bit_rate(VideoEncoding::JPEG2000, _maximum_j2k_video_bit_rate->GetValue() * 1000000);
	}

	void maximum_mpeg2_video_bit_rate_changed()
	{
		Config::instance()->set_maximum_video_bit_rate(VideoEncoding::MPEG2, _maximum_mpeg2_video_bit_rate->GetValue() * 1000000);
	}

	void allow_any_dcp_frame_rate_changed()
	{
		Config::instance()->set_allow_any_dcp_frame_rate(_allow_any_dcp_frame_rate->GetValue());
	}

	void allow_any_container_changed()
	{
		Config::instance()->set_allow_any_container(_allow_any_container->GetValue());
	}

	void allow_96khz_audio_changed()
	{
		Config::instance()->set_allow_96hhz_audio(_allow_96khz_audio->GetValue());
	}

	void use_all_channels_changed()
	{
		Config::instance()->set_use_all_audio_channels(_use_all_audio_channels->GetValue());
	}

	void allow_smpte_bv20_changed()
	{
		Config::instance()->set_allow_smpte_bv20(_allow_smpte_bv20->GetValue());
	}

	void isdcf_name_part_length_changed()
	{
		Config::instance()->set_isdcf_name_part_length(_isdcf_name_part_length->GetValue());
	}

	wxSpinCtrl* _maximum_j2k_video_bit_rate = nullptr;
	wxSpinCtrl* _maximum_mpeg2_video_bit_rate = nullptr;
	CheckBox* _allow_any_dcp_frame_rate = nullptr;
	CheckBox* _allow_any_container = nullptr;
	CheckBox* _allow_96khz_audio = nullptr;
	CheckBox* _use_all_audio_channels = nullptr;
	CheckBox* _allow_smpte_bv20 = nullptr;
	wxSpinCtrl* _isdcf_name_part_length = nullptr;
};



/** @class AdvancedPage
 *  @brief Advanced page of the preferences dialog.
 */
class AdvancedPage : public preferences::Page
{
public:
	AdvancedPage(wxSize panel_size, int border)
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
		wxStaticText* m = new StaticText(parent, text);
		table->Add(m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer(table, _panel, _("Video display mode"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_video_display_mode = new wxChoice(_panel, wxID_ANY);
		table->Add(_video_display_mode);

		auto restart = add_label_to_sizer(table, _panel, variant::wx::insert_dcpomatic(_("(restart %s to change display mode)")), false);
		auto font = restart->GetFont();
		font.SetStyle(wxFONTSTYLE_ITALIC);
		font.SetPointSize(font.GetPointSize() - 1);
		restart->SetFont(font);
		table->AddSpacer(0);

		_show_experimental_audio_processors = new CheckBox(_panel, _("Show experimental audio processors"));
		table->Add(_show_experimental_audio_processors, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
		table->AddSpacer(0);

		_only_servers_encode = new CheckBox(_panel, _("Only servers encode"));
		table->Add(_only_servers_encode, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
		table->AddSpacer(0);

		_layout_for_short_screen = new CheckBox(_panel, _("Layout for short screen"));
		table->Add(_layout_for_short_screen, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
		table->AddSpacer(0);

		{
			add_label_to_sizer(table, _panel, _("Maximum number of frames to store per thread"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
			auto s = new wxBoxSizer(wxHORIZONTAL);
			_frames_in_memory_multiplier = new wxSpinCtrl(_panel);
			s->Add(_frames_in_memory_multiplier, 1);
			table->Add(s, 1);
		}

		{
			auto format = create_label(_panel, _("DCP metadata filename format"), true);
#ifdef DCPOMATIC_OSX
			auto align = new wxBoxSizer(wxHORIZONTAL);
			align->Add(format, 0, wxTOP, 2);
			table->Add(align, 0, wxALIGN_RIGHT | wxRIGHT, DCPOMATIC_SIZER_GAP - 2);
#else
			table->Add(format, 0, wxTOP | wxLEFT | wxRIGHT | wxALIGN_TOP, DCPOMATIC_SIZER_GAP);
#endif
			dcp::NameFormat::Map titles;
			titles['t'] = wx_to_std(_("type(cpl/pkl)"));
			dcp::NameFormat::Map examples;
			examples['t'] = "cpl";
			_dcp_metadata_filename_format = new NameFormatEditor(
				_panel, Config::instance()->dcp_metadata_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.xml"
				);
			table->Add(_dcp_metadata_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			auto format = create_label(_panel, _("DCP asset filename format"), true);
#ifdef DCPOMATIC_OSX
			auto align = new wxBoxSizer(wxHORIZONTAL);
			align->Add(format, 0, wxTOP, 2);
			table->Add(align, 0, wxALIGN_RIGHT | wxRIGHT, DCPOMATIC_SIZER_GAP - 2);
#else
			table->Add(format, 0, wxTOP | wxLEFT | wxRIGHT | wxALIGN_TOP, DCPOMATIC_SIZER_GAP);
#endif
			dcp::NameFormat::Map titles;
			titles['t'] = wx_to_std(_("type (j2c/pcm/sub)"));
			titles['r'] = wx_to_std(_("reel number"));
			titles['n'] = wx_to_std(_("number of reels"));
			titles['c'] = wx_to_std(_("content filename"));
			dcp::NameFormat::Map examples;
			examples['t'] = "j2c";
			examples['r'] = "1";
			examples['n'] = "4";
			examples['c'] = "myfile.mp4";
			_dcp_asset_filename_format = new NameFormatEditor(
				_panel, Config::instance()->dcp_asset_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.mxf"
				);
			table->Add(_dcp_asset_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			add_top_aligned_label_to_sizer(table, _panel, _("Log"));
			auto t = new wxFlexGridSizer(2);
			_log_general = new CheckBox(_panel, _("General"));
			t->Add(_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new CheckBox(_panel, _("Warnings"));
			t->Add(_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new CheckBox(_panel, _("Errors"));
			t->Add(_log_error, 1, wxEXPAND | wxALL);
			/// TRANSLATORS: translate the word "Timing" here; do not include the "Config|" prefix
			_log_timing = new CheckBox(_panel, S_("Config|Timing"));
			t->Add(_log_timing, 1, wxEXPAND | wxALL);
			_log_debug_threed = new CheckBox(_panel, _("Debug: 3D"));
			t->Add(_log_debug_threed, 1, wxEXPAND | wxALL);
			_log_debug_encode = new CheckBox(_panel, _("Debug: encode"));
			t->Add(_log_debug_encode, 1, wxEXPAND | wxALL);
			_log_debug_email = new CheckBox(_panel, _("Debug: email sending"));
			t->Add(_log_debug_email, 1, wxEXPAND | wxALL);
			_log_debug_video_view = new CheckBox(_panel, _("Debug: video view"));
			t->Add(_log_debug_video_view, 1, wxEXPAND | wxALL);
			_log_debug_player = new CheckBox(_panel, _("Debug: player"));
			t->Add(_log_debug_player, 1, wxEXPAND | wxALL);
			_log_debug_audio_analysis = new CheckBox(_panel, _("Debug: audio analysis"));
			t->Add(_log_debug_audio_analysis, 1, wxEXPAND | wxALL);
			table->Add(t, 0, wxALL, 6);
		}

#ifdef DCPOMATIC_WINDOWS
		_win32_console = new CheckBox(_panel, _("Open console window"));
		table->Add(_win32_console, 1, wxEXPAND | wxALL);
		table->AddSpacer(0);
#endif

		_video_display_mode->Append(_("Simple (safer)"));
#if wxCHECK_VERSION(3, 1, 0)
		_video_display_mode->Append(_("OpenGL (faster)"));
#endif
		_video_display_mode->Bind(wxEVT_CHOICE, boost::bind(&AdvancedPage::video_display_mode_changed, this));
		_show_experimental_audio_processors->bind(&AdvancedPage::show_experimental_audio_processors_changed, this);
		_only_servers_encode->bind(&AdvancedPage::only_servers_encode_changed, this);
		_layout_for_short_screen->bind(&AdvancedPage::layout_for_short_screen_changed, this);
		_frames_in_memory_multiplier->Bind(wxEVT_SPINCTRL, boost::bind(&AdvancedPage::frames_in_memory_multiplier_changed, this));
		_dcp_metadata_filename_format->Changed.connect(boost::bind(&AdvancedPage::dcp_metadata_filename_format_changed, this));
		_dcp_asset_filename_format->Changed.connect(boost::bind(&AdvancedPage::dcp_asset_filename_format_changed, this));
		_log_general->bind(&AdvancedPage::log_changed, this);
		_log_warning->bind(&AdvancedPage::log_changed, this);
		_log_error->bind(&AdvancedPage::log_changed, this);
		_log_timing->bind(&AdvancedPage::log_changed, this);
		_log_debug_threed->bind(&AdvancedPage::log_changed, this);
		_log_debug_encode->bind(&AdvancedPage::log_changed, this);
		_log_debug_email->bind(&AdvancedPage::log_changed, this);
		_log_debug_video_view->bind(&AdvancedPage::log_changed, this);
		_log_debug_player->bind(&AdvancedPage::log_changed, this);
		_log_debug_audio_analysis->bind(&AdvancedPage::log_changed, this);
#ifdef DCPOMATIC_WINDOWS
		_win32_console->bind(&AdvancedPage::win32_console_changed, this);
#endif
	}

	void config_changed() override
	{
		auto config = Config::instance();

		switch(config->video_view_type()) {
		case Config::VIDEO_VIEW_SIMPLE:
			checked_set(_video_display_mode, 0);
			break;
		case Config::VIDEO_VIEW_OPENGL:
			checked_set(_video_display_mode, 1);
			break;
		}
		checked_set(_show_experimental_audio_processors, config->show_experimental_audio_processors());
		checked_set(_only_servers_encode, config->only_servers_encode());
		checked_set(_layout_for_short_screen, config->layout_for_short_screen());
		checked_set(_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set(_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set(_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set(_log_timing, config->log_types() & LogEntry::TYPE_TIMING);
		checked_set(_log_debug_threed, config->log_types() & LogEntry::TYPE_DEBUG_THREE_D);
		checked_set(_log_debug_encode, config->log_types() & LogEntry::TYPE_DEBUG_ENCODE);
		checked_set(_log_debug_email, config->log_types() & LogEntry::TYPE_DEBUG_EMAIL);
		checked_set(_log_debug_video_view, config->log_types() & LogEntry::TYPE_DEBUG_VIDEO_VIEW);
		checked_set(_log_debug_player, config->log_types() & LogEntry::TYPE_DEBUG_PLAYER);
		checked_set(_log_debug_audio_analysis, config->log_types() & LogEntry::TYPE_DEBUG_AUDIO_ANALYSIS);
		checked_set(_frames_in_memory_multiplier, config->frames_in_memory_multiplier());
#ifdef DCPOMATIC_WINDOWS
		checked_set(_win32_console, config->win32_console());
#endif
	}

	void video_display_mode_changed()
	{
		if (_video_display_mode->GetSelection() == 0) {
			Config::instance()->set_video_view_type(Config::VIDEO_VIEW_SIMPLE);
		} else {
			Config::instance()->set_video_view_type(Config::VIDEO_VIEW_OPENGL);
		}
	}

	void frames_in_memory_multiplier_changed()
	{
		Config::instance()->set_frames_in_memory_multiplier(_frames_in_memory_multiplier->GetValue());
	}

	void show_experimental_audio_processors_changed()
	{
		Config::instance()->set_show_experimental_audio_processors(_show_experimental_audio_processors->GetValue());
	}

	void only_servers_encode_changed()
	{
		Config::instance()->set_only_servers_encode(_only_servers_encode->GetValue());
	}

	void layout_for_short_screen_changed()
	{
		Config::instance()->set_layout_for_short_screen(_layout_for_short_screen->GetValue());
	}

	void dcp_metadata_filename_format_changed()
	{
		Config::instance()->set_dcp_metadata_filename_format(_dcp_metadata_filename_format->get());
	}

	void dcp_asset_filename_format_changed()
	{
		Config::instance()->set_dcp_asset_filename_format(_dcp_asset_filename_format->get());
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
		if (_log_debug_threed->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_THREE_D;
		}
		if (_log_debug_encode->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_ENCODE;
		}
		if (_log_debug_email->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_EMAIL;
		}
		if (_log_debug_video_view->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_VIDEO_VIEW;
		}
		if (_log_debug_player->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_PLAYER;
		}
		if (_log_debug_audio_analysis->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_AUDIO_ANALYSIS;
		}
		Config::instance()->set_log_types(types);
	}

#ifdef DCPOMATIC_WINDOWS
	void win32_console_changed()
	{
		Config::instance()->set_win32_console(_win32_console->GetValue());
	}
#endif

	wxChoice* _video_display_mode = nullptr;
	wxSpinCtrl* _frames_in_memory_multiplier = nullptr;
	CheckBox* _show_experimental_audio_processors = nullptr;
	CheckBox* _only_servers_encode = nullptr;
	CheckBox* _layout_for_short_screen = nullptr;
	NameFormatEditor* _dcp_metadata_filename_format = nullptr;
	NameFormatEditor* _dcp_asset_filename_format = nullptr;
	CheckBox* _log_general = nullptr;
	CheckBox* _log_warning = nullptr;
	CheckBox* _log_error = nullptr;
	CheckBox* _log_timing = nullptr;
	CheckBox* _log_debug_threed = nullptr;
	CheckBox* _log_debug_encode = nullptr;
	CheckBox* _log_debug_email = nullptr;
	CheckBox* _log_debug_video_view = nullptr;
	CheckBox* _log_debug_player = nullptr;
	CheckBox* _log_debug_audio_analysis = nullptr;
#ifdef DCPOMATIC_WINDOWS
	CheckBox* _win32_console = nullptr;
#endif
};


wxPreferencesEditor*
create_full_config_dialog()
{
	auto e = new wxPreferencesEditor();

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	wxSize ps = wxSize(900, -1);
	int const border = 16;
#else
	wxSize ps = wxSize(-1, -1);
	int const border = 8;
#endif

	e->AddPage(new FullGeneralPage(ps, border));
	e->AddPage(new preferences::SoundPage(ps, border));
	e->AddPage(new DefaultsPage(ps, border));
	e->AddPage(new EncodingServersPage(ps, border));
#ifdef DCPOMATIC_GROK
	e->AddPage(new GPUPage(ps, border));
#endif
	e->AddPage(new preferences::KeysPage(ps, border));
	e->AddPage(new TMSPage(ps, border));
	e->AddPage(new preferences::EmailPage(ps, border));
	e->AddPage(new preferences::KDMEmailPage(ps, border));
	e->AddPage(new NotificationsPage(ps, border));
	e->AddPage(new CoverSheetPage(ps, border));
	e->AddPage(new IdentifiersPage(ps, border));
	e->AddPage(new NonStandardPage(ps, border));
	e->AddPage(new AdvancedPage(ps, border));
	return e;
}
