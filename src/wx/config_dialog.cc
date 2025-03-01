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


#include "audio_backend.h"
#include "audio_mapping_view.h"
#include "certificate_chain_editor.h"
#include "check_box.h"
#include "config_dialog.h"
#include "config_move_dialog.h"
#include "dcpomatic_button.h"
#include "file_picker_ctrl.h"
#include "nag_dialog.h"
#include "static_text.h"
#include "wx_variant.h"
#include "lib/constants.h"
#include "lib/util.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <fmt/format.h>


using std::function;
using std::make_pair;
using std::make_shared;
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
using namespace dcpomatic::preferences;


static
bool
do_nothing()
{
	return false;
}

Page::Page(wxSize panel_size, int border)
	: _border(border)
	, _panel(nullptr)
	, _panel_size(panel_size)
	, _window_exists(false)
{
	_config_connection = Config::instance()->Changed.connect(bind(&Page::config_changed_wrapper, this));
}


wxWindow*
Page::CreateWindow(wxWindow* parent)
{
	return create_window(parent);
}


wxWindow*
Page::create_window(wxWindow* parent)
{
	_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, _panel_size);
	auto s = new wxBoxSizer(wxVERTICAL);
	_panel->SetSizer(s);

	setup();
	_window_exists = true;
	config_changed();

	_panel->Bind(wxEVT_DESTROY, bind(&Page::window_destroyed, this));

	return _panel;
}

void
Page::config_changed_wrapper()
{
	if (_window_exists) {
		config_changed();
	}
}

void
Page::window_destroyed()
{
	_window_exists = false;
}


GeneralPage::GeneralPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}


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
	_config_file->Bind (wxEVT_FILEPICKER_CHANGED, boost::bind(&GeneralPage::config_file_changed,  this));
	_cinemas_file->Bind(wxEVT_FILEPICKER_CHANGED, boost::bind(&GeneralPage::cinemas_file_changed, this));
}


void
GeneralPage::config_file_changed()
{
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

	checked_set(_config_file, config->config_read_file());
	checked_set(_cinemas_file, config->cinemas_file());

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

wxString
KeysPage::GetName() const
{
	return _("Keys");
}

void
KeysPage::setup()
{
	wxFont subheading_font(*wxNORMAL_FONT);
	subheading_font.SetWeight(wxFONTWEIGHT_BOLD);

	auto sizer = _panel->GetSizer();

	{
		auto m = new StaticText(_panel, _("Decrypting KDMs"));
		m->SetFont(subheading_font);
		sizer->Add(m, 0, wxALL | wxEXPAND, _border);
	}

	auto kdm_buttons = new wxBoxSizer(wxVERTICAL);

	auto export_decryption_certificate = new Button(_panel, _("Export KDM decryption leaf certificate..."));
	kdm_buttons->Add(export_decryption_certificate, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto export_settings = new Button(_panel, _("Export all KDM decryption settings..."));
	kdm_buttons->Add(export_settings, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto import_settings = new Button(_panel, _("Import all KDM decryption settings..."));
	kdm_buttons->Add(import_settings, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto decryption_advanced = new Button(_panel, _("Advanced..."));
	kdm_buttons->Add(decryption_advanced, 0);

	sizer->Add(kdm_buttons, 0, wxLEFT, _border);

	export_decryption_certificate->Bind(wxEVT_BUTTON, bind(&KeysPage::export_decryption_certificate, this));
	export_settings->Bind(wxEVT_BUTTON, bind(&KeysPage::export_decryption_chain_and_key, this));
	import_settings->Bind(wxEVT_BUTTON, bind(&KeysPage::import_decryption_chain_and_key, this));
	decryption_advanced->Bind(wxEVT_BUTTON, bind(&KeysPage::decryption_advanced, this));

	{
		auto m = new StaticText(_panel, _("Signing DCPs and KDMs"));
		m->SetFont(subheading_font);
		sizer->Add(m, 0, wxALL | wxEXPAND, _border);
	}

	auto signing_buttons = new wxBoxSizer(wxVERTICAL);

	auto signing_advanced = new Button(_panel, _("Advanced..."));
	signing_buttons->Add(signing_advanced, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto remake_signing = new Button(_panel, _("Re-make certificates and key..."));
	signing_buttons->Add(remake_signing, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	sizer->Add(signing_buttons, 0, wxLEFT, _border);

	signing_advanced->Bind(wxEVT_BUTTON, bind(&KeysPage::signing_advanced, this));
	remake_signing->Bind(wxEVT_BUTTON, bind(&KeysPage::remake_signing, this));
}


void
KeysPage::remake_signing()
{
	MakeChainDialog dialog(_panel, Config::instance()->signer_chain());

	if (dialog.ShowModal() == wxID_OK) {
		Config::instance()->set_signer_chain(dialog.get());
	}
}


void
KeysPage::decryption_advanced()
{
	CertificateChainEditor editor(
		_panel, _("Decrypting KDMs"), _border,
		bind(&Config::set_decryption_chain, Config::instance(), _1),
		bind(&Config::decryption_chain, Config::instance()),
		bind(&KeysPage::nag_alter_decryption_chain, this)
		);

	editor.ShowModal();
}

void
KeysPage::signing_advanced()
{
	CertificateChainEditor editor(
		_panel, _("Signing DCPs and KDMs"), _border,
		bind(&Config::set_signer_chain, Config::instance(), _1),
		bind(&Config::signer_chain, Config::instance()),
		bind(&do_nothing)
		);

	editor.ShowModal();
}

void
KeysPage::export_decryption_chain_and_key()
{
	wxFileDialog dialog(
		_panel, _("Select Export File"), wxEmptyString, wxEmptyString, char_to_wx("DOM files (*.dom)|*.dom"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(dialog.GetPath()));
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, f.open_error(), OpenFileError::WRITE);
	}

	auto const chain = Config::instance()->decryption_chain()->chain();
	f.checked_write(chain.c_str(), chain.length());
	auto const key = Config::instance()->decryption_chain()->key();
	DCPOMATIC_ASSERT(key);
	f.checked_write(key->c_str(), key->length());
}

void
KeysPage::import_decryption_chain_and_key()
{
	if (NagDialog::maybe_nag(
		    _panel,
		    Config::NAG_IMPORT_DECRYPTION_CHAIN,
		    _("If you continue with this operation you will no longer be able to use any DKDMs that you have created with the current certificates and key.  Also, any KDMs that have been sent to you for those certificates will become useless.  Proceed with caution!"),
		    true
		    )) {
		return;
	}

	wxFileDialog dialog(
		_panel, _("Select File To Import"), wxEmptyString, wxEmptyString, char_to_wx("DOM files (*.dom)|*.dom")
		);

	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	auto new_chain = make_shared<dcp::CertificateChain>();

	dcp::File f(wx_to_std(dialog.GetPath()), "r");
	if (!f) {
		throw OpenFileError(f.path(), f.open_error(), OpenFileError::WRITE);
	}

	string current;
	while (!f.eof()) {
		char buffer[128];
		if (f.gets(buffer, 128) == 0) {
			break;
		}
		current += buffer;
		if (strncmp (buffer, "-----END CERTIFICATE-----", 25) == 0) {
			new_chain->add(dcp::Certificate(current));
			current = "";
		} else if (strncmp(buffer, "-----END RSA PRIVATE KEY-----", 29) == 0) {
			new_chain->set_key(current);
			current = "";
		}
	}

	if (new_chain->chain_valid() && new_chain->private_key_valid()) {
		Config::instance()->set_decryption_chain(new_chain);
	} else {
		error_dialog(_panel, variant::wx::insert_dcpomatic(_("Invalid %s export file")));
	}
}

bool
KeysPage::nag_alter_decryption_chain()
{
	return NagDialog::maybe_nag(
		_panel,
		Config::NAG_ALTER_DECRYPTION_CHAIN,
		_("If you continue with this operation you will no longer be able to use any DKDMs that you have created.  Also, any KDMs that have been sent to you will become useless.  Proceed with caution!"),
		true
		);
}

void
KeysPage::export_decryption_certificate()
{
	auto config = Config::instance();
	wxString default_name = char_to_wx("dcpomatic");
	if (!config->dcp_creator().empty()) {
		default_name += char_to_wx("_") + std_to_wx(careful_string_filter(config->dcp_creator()));
	}
	if (!config->dcp_issuer().empty()) {
		default_name += char_to_wx("_") + std_to_wx(careful_string_filter(config->dcp_issuer()));
	}
	default_name += char_to_wx("_kdm_decryption_cert.pem");

	wxFileDialog dialog(
		_panel, _("Select Certificate File"), wxEmptyString, default_name, char_to_wx("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(dialog.GetPath()));
	if (path.extension() != ".pem") {
		path += ".pem";
	}
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, f.open_error(), OpenFileError::WRITE);
	}

	auto const s = Config::instance()->decryption_chain()->leaf().certificate(true);
	f.checked_write(s.c_str(), s.length());
}

wxString
SoundPage::GetName() const
{
	return _("Sound");
}

void
SoundPage::setup()
{
	auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

	int r = 0;

	_sound = new CheckBox(_panel, _("Play sound via"));
	table->Add(_sound, wxGBPosition(r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* s = new wxBoxSizer(wxHORIZONTAL);
	_sound_output = new wxChoice(_panel, wxID_ANY);
	s->Add(_sound_output, 0);
	_sound_output_details = new wxStaticText(_panel, wxID_ANY, {});
	s->Add(_sound_output_details, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, DCPOMATIC_SIZER_X_GAP);
	table->Add(s, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("Mapping"), true, wxGBPosition(r, 0));
	_map = new AudioMappingView(_panel, _("DCP"), _("DCP"), _("Output"), _("output"));
	table->Add(_map, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	_reset_to_default = new Button(_panel, _("Reset to default"));
	table->Add(_reset_to_default, wxGBPosition(r, 1));
	++r;

	wxFont font = _sound_output_details->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_sound_output_details->SetFont(font);

	for (auto name: AudioBackend::instance()->output_device_names()) {
		_sound_output->Append(std_to_wx(name));
	}

	_sound->bind(&SoundPage::sound_changed, this);
	_sound_output->Bind(wxEVT_CHOICE, bind(&SoundPage::sound_output_changed, this));
	_map->Changed.connect(bind(&SoundPage::map_changed, this, _1));
	_reset_to_default->Bind(wxEVT_BUTTON, bind(&SoundPage::reset_to_default, this));
}

void
SoundPage::reset_to_default()
{
	Config::instance()->set_audio_mapping_to_default();
}

void
SoundPage::map_changed(AudioMapping m)
{
	Config::instance()->set_audio_mapping(m);
}

void
SoundPage::sound_changed()
{
	Config::instance()->set_sound(_sound->GetValue());
}

void
SoundPage::sound_output_changed()
{
	auto const so = get_sound_output();
	auto default_device = AudioBackend::instance()->default_device_name();

	if (!so || so == default_device) {
		Config::instance()->unset_sound_output();
	} else {
		Config::instance()->set_sound_output(*so);
	}
}

void
SoundPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_sound, config->sound());

	auto const current_so = get_sound_output();
	optional<string> configured_so;

	auto& audio = AudioBackend::instance()->rtaudio();

	if (config->sound_output()) {
		configured_so = config->sound_output().get();
	} else {
		/* No configured output means we should use the default */
		configured_so = AudioBackend::instance()->default_device_name();
	}

	if (configured_so && current_so != configured_so) {
		/* Update _sound_output with the configured value */
		unsigned int i = 0;
		while (i < _sound_output->GetCount()) {
			if (_sound_output->GetString(i) == std_to_wx(*configured_so)) {
				_sound_output->SetSelection(i);
				break;
			}
			++i;
		}
	}

	map<int, wxString> apis;
	apis[RtAudio::MACOSX_CORE]    = _("CoreAudio");
	apis[RtAudio::WINDOWS_ASIO]   = _("ASIO");
	apis[RtAudio::WINDOWS_DS]     = _("Direct Sound");
	apis[RtAudio::WINDOWS_WASAPI] = _("WASAPI");
	apis[RtAudio::UNIX_JACK]      = _("JACK");
	apis[RtAudio::LINUX_ALSA]     = _("ALSA");
	apis[RtAudio::LINUX_PULSE]    = _("PulseAudio");
	apis[RtAudio::LINUX_OSS]      = _("OSS");
	apis[RtAudio::RTAUDIO_DUMMY]  = _("Dummy");

	int const channels = configured_so ? AudioBackend::instance()->device_output_channels(*configured_so).get_value_or(0) : 0;

	_sound_output_details->SetLabel(
		wxString::Format(_("%d channels on %s"), channels, apis[audio.getCurrentApi()])
		);

	_map->set(Config::instance()->audio_mapping(channels));

	vector<NamedChannel> input;
	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		input.push_back(NamedChannel(short_audio_channel_name(i), i));
	}
	_map->set_input_channels(input);

	vector<NamedChannel> output;
	for (int i = 0; i < channels; ++i) {
		output.push_back(NamedChannel(fmt::to_string(i), i));
	}
	_map->set_output_channels(output);

	setup_sensitivity();
}

void
SoundPage::setup_sensitivity()
{
	_sound_output->Enable(_sound->GetValue());
}

/** @return Currently-selected preview sound output in the dialogue */
optional<string>
SoundPage::get_sound_output()
{
	int const sel = _sound_output->GetSelection();
	if (sel == wxNOT_FOUND) {
		return {};
	}

	return wx_to_std(_sound_output->GetString(sel));
}


LocationsPage::LocationsPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}

wxString
LocationsPage::GetName() const
{
	return _("Locations");
}

#ifdef DCPOMATIC_OSX
wxBitmap
LocationsPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("locations"), wxBITMAP_TYPE_PNG);
}
#endif

void
LocationsPage::setup()
{
	int r = 0;

	auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

	add_label_to_sizer(table, _panel, _("Content directory"), true, wxGBPosition(r, 0));
	_content_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1));
	table->Add(_content_directory, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("Playlist directory"), true, wxGBPosition(r, 0));
	_playlist_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1));
	table->Add(_playlist_directory, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("KDM directory"), true, wxGBPosition(r, 0));
	_kdm_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1));
	table->Add(_kdm_directory, wxGBPosition(r, 1));
	++r;

	_content_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::content_directory_changed, this));
	_playlist_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::playlist_directory_changed, this));
	_kdm_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::kdm_directory_changed, this));
}

void
LocationsPage::config_changed()
{
	auto config = Config::instance();

	if (config->player_content_directory()) {
		checked_set(_content_directory, *config->player_content_directory());
	}
	if (config->player_playlist_directory()) {
		checked_set(_playlist_directory, *config->player_playlist_directory());
	}
	if (config->player_kdm_directory()) {
		checked_set(_kdm_directory, *config->player_kdm_directory());
	}
}

void
LocationsPage::content_directory_changed()
{
	Config::instance()->set_player_content_directory(wx_to_std(_content_directory->GetPath()));
}

void
LocationsPage::playlist_directory_changed()
{
	Config::instance()->set_player_playlist_directory(wx_to_std(_playlist_directory->GetPath()));
}

void
LocationsPage::kdm_directory_changed()
{
	Config::instance()->set_player_kdm_directory(wx_to_std(_kdm_directory->GetPath()));
}
