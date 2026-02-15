/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "config_move_dialog.h"
#include "dcpomatic_button.h"
#include "file_picker_ctrl.h"
#include "general_preferences_page.h"
#include "preferences_page.h"
#include "wx_util.h"
#include "wx_variant.h"
#include <dcp/filesystem.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>


using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::vector;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;


GeneralPage::GeneralPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}


#ifdef DCPOMATIC_OSX
wxBitmap
GeneralPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("general"), wxBITMAP_TYPE_PNG);
}
#endif


wxString
GeneralPage::GetName() const
{
	return _("General");
}


void
GeneralPage::add_language_controls(wxGridBagSizer* table, int& r)
{
	_set_language = new CheckBox(_panel, _("Set language"));
	table->Add(_set_language, wxGBPosition(r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_language = new wxChoice(_panel, wxID_ANY);
	vector<pair<string, string>> languages;
	languages.push_back(make_pair("Čeština", "cs_CZ"));
	languages.push_back(make_pair("汉语/漢語", "zh_CN"));
	languages.push_back(make_pair("Dansk", "da_DK"));
	languages.push_back(make_pair("Deutsch", "de_DE"));
	languages.push_back(make_pair("English", "en_GB"));
	languages.push_back(make_pair("Español", "es_ES"));
	languages.push_back(make_pair("فارسی", "fa_IR"));
	languages.push_back(make_pair("Français", "fr_FR"));
	languages.push_back(make_pair("Italiano", "it_IT"));
	languages.push_back(make_pair("日本語", "ja_JP"));
	languages.push_back(make_pair("한국어", "ko_KR"));
	languages.push_back(make_pair("Nederlands", "nl_NL"));
	languages.push_back(make_pair("Русский", "ru_RU"));
	languages.push_back(make_pair("Polski", "pl_PL"));
	languages.push_back(make_pair("Português europeu", "pt_PT"));
	languages.push_back(make_pair("Português do Brasil", "pt_BR"));
	languages.push_back(make_pair("Svenska", "sv_SE"));
	languages.push_back(make_pair("Slovenščina", "sl_SI"));
	languages.push_back(make_pair("Slovenský jazyk", "sk_SK"));
	// languages.push_back(make_pair("Türkçe", "tr_TR"));
	languages.push_back(make_pair("українська мова", "uk_UA"));
	languages.push_back(make_pair("Magyar nyelv", "hu_HU"));
	checked_set(_language, languages);
	table->Add(_language, wxGBPosition(r, 1));
	++r;

	auto restart = add_label_to_sizer(
		table, _panel, variant::wx::insert_dcpomatic(_("(restart %s to see language changes)")), false, wxGBPosition(r, 0), wxGBSpan(1, 2)
		);
	wxFont font = restart->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	restart->SetFont(font);
	++r;

	_set_language->bind(&GeneralPage::set_language_changed, this);
	_language->Bind(wxEVT_CHOICE, bind(&GeneralPage::language_changed, this));
}


void
GeneralPage::add_config_file_controls(wxGridBagSizer* table, int& r)
{
	add_label_to_sizer(table, _panel, _("Configuration file"), true, wxGBPosition(r, 0));
	_config_file = new FilePickerCtrl(_panel, _("Select configuration file"), char_to_wx("*.xml"), true, false, "ConfigFilePath");
	table->Add(_config_file, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("Cinema and screen database file"), true, wxGBPosition(r, 0));
	_cinemas_file = new FilePickerCtrl(_panel, _("Select cinema and screen database file"), char_to_wx("*.sqlite3"), true, false, "CinemaDatabasePath");
	table->Add(_cinemas_file, wxGBPosition(r, 1));
	auto export_cinemas = new Button(_panel, _("Export..."));
	table->Add(export_cinemas, wxGBPosition(r, 2));
	++r;

	export_cinemas->Bind(wxEVT_BUTTON, boost::bind(&GeneralPage::export_cinemas_file, this));
	_config_file->Bind(wxEVT_FILEPICKER_CHANGED, boost::bind(&GeneralPage::config_file_changed,  this));
	_cinemas_file->Bind(wxEVT_FILEPICKER_CHANGED, boost::bind(&GeneralPage::cinemas_file_changed, this));
}


void
GeneralPage::config_file_changed()
{
	if (!_config_file) {
		return;
	}

	auto config = Config::instance();
	auto const new_file = _config_file->path();
	if (!new_file || *new_file == config->config_read_file()) {
		return;
	}
	bool copy_and_link = true;
	if (dcp::filesystem::exists(*new_file)) {
		ConfigMoveDialog dialog(_panel, *new_file);
		if (dialog.ShowModal() == wxID_OK) {
			copy_and_link = false;
		}
	}

	if (copy_and_link) {
		config->write();
		if (new_file != config->config_read_file()) {
			config->copy_and_link(*new_file);
		}
	} else {
		config->link(*new_file);
	}
}

void
GeneralPage::cinemas_file_changed()
{
	if (!_cinemas_file) {
		return;
	}

	if (auto path = _cinemas_file->path()) {
		Config::instance()->set_cinemas_file(*path);
	}
}


void
GeneralPage::export_cinemas_file()
{
	wxFileDialog dialog(
		_panel, _("Select Cinemas File"), wxEmptyString, wxEmptyString, char_to_wx("SQLite files (*.sqlite3)|*.sqlite3"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
	);

	if (dialog.ShowModal() == wxID_OK) {
		dcp::filesystem::copy_file(Config::instance()->cinemas_file(), wx_to_std(dialog.GetPath()), dcp::filesystem::CopyOptions::OVERWRITE_EXISTING);
	}
}


void
GeneralPage::add_update_controls(wxGridBagSizer* table, int& r)
{
	_check_for_updates = new CheckBox(_panel, _("Check for updates on startup"));
	table->Add(_check_for_updates, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	_check_for_test_updates = new CheckBox(_panel, _("Check for testing updates on startup"));
	table->Add(_check_for_test_updates, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	_check_for_updates->bind(&GeneralPage::check_for_updates_changed, this);
	_check_for_test_updates->bind(&GeneralPage::check_for_test_updates_changed, this);
}

void
GeneralPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_set_language, static_cast<bool>(config->language()));

	/* Backwards compatibility of config file */

	map<string, string> compat_map;
	compat_map["fr"] = "fr_FR";
	compat_map["it"] = "it_IT";
	compat_map["es"] = "es_ES";
	compat_map["sv"] = "sv_SE";
	compat_map["de"] = "de_DE";
	compat_map["nl"] = "nl_NL";
	compat_map["ru"] = "ru_RU";
	compat_map["pl"] = "pl_PL";
	compat_map["da"] = "da_DK";
	compat_map["pt"] = "pt_PT";
	compat_map["sk"] = "sk_SK";
	compat_map["cs"] = "cs_CZ";
	compat_map["uk"] = "uk_UA";

	auto lang = config->language().get_value_or("en_GB");
	if (compat_map.find(lang) != compat_map.end()) {
		lang = compat_map[lang];
	}

	checked_set(_language, lang);

	if (_config_file) {
		checked_set(_config_file, config->config_read_file());
	}
	if (_cinemas_file) {
		checked_set(_cinemas_file, config->cinemas_file());
	}

	checked_set(_check_for_updates, config->check_for_updates());
	checked_set(_check_for_test_updates, config->check_for_test_updates());

	setup_sensitivity();
}

void
GeneralPage::setup_sensitivity()
{
	_language->Enable(_set_language->GetValue());
	_check_for_test_updates->Enable(_check_for_updates->GetValue());
}

void
GeneralPage::set_language_changed()
{
	setup_sensitivity();
	if (_set_language->GetValue()) {
		language_changed();
	} else {
		Config::instance()->unset_language();
	}
}

void
GeneralPage::language_changed()
{
	int const sel = _language->GetSelection();
	if (sel != -1) {
		Config::instance()->set_language(string_client_data(_language->GetClientObject(sel)));
	} else {
		Config::instance()->unset_language();
	}
}

void
GeneralPage::check_for_updates_changed()
{
	Config::instance()->set_check_for_updates(_check_for_updates->GetValue());
}

void
GeneralPage::check_for_test_updates_changed()
{
	Config::instance()->set_check_for_test_updates(_check_for_test_updates->GetValue());
}


