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

#include "config_dialog.h"
#include "nag_dialog.h"

using std::string;
using boost::bind;
using boost::optional;
using boost::shared_ptr;

static
void
do_nothing ()
{

}

Page::Page (wxSize panel_size, int border)
	: _border (border)
	, _panel (0)
	, _panel_size (panel_size)
	, _window_exists (false)
{
	_config_connection = Config::instance()->Changed.connect (boost::bind (&Page::config_changed_wrapper, this));
}

wxWindow*
Page::create_window (wxWindow* parent)
{
	_panel = new wxPanel (parent, wxID_ANY, wxDefaultPosition, _panel_size);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (s);

	setup ();
	_window_exists = true;
	config_changed ();

	_panel->Bind (wxEVT_DESTROY, boost::bind (&Page::window_destroyed, this));

	return _panel;
}

void
Page::config_changed_wrapper ()
{
	if (_window_exists) {
		config_changed ();
	}
}

void
Page::window_destroyed ()
{
	_window_exists = false;
}


StockPage::StockPage (Kind kind, wxSize panel_size, int border)
	: wxStockPreferencesPage (kind)
	, Page (panel_size, border)
{

}

wxWindow*
StockPage::CreateWindow (wxWindow* parent)
{
	return create_window (parent);
}

StandardPage::StandardPage (wxSize panel_size, int border)
	: Page (panel_size, border)
{

}

wxWindow*
StandardPage::CreateWindow (wxWindow* parent)
{
	return create_window (parent);
}

GeneralPage::GeneralPage (wxSize panel_size, int border)
	: StockPage (Kind_General, panel_size, border)
{

}

void
GeneralPage::add_language_controls (wxGridBagSizer* table, int& r)
{
	_set_language = new wxCheckBox (_panel, wxID_ANY, _("Set language"));
	table->Add (_set_language, wxGBPosition (r, 0));
	_language = new wxChoice (_panel, wxID_ANY);
	std::vector<std::pair<std::string, std::string> > languages;
	languages.push_back (std::make_pair ("Čeština", "cs_CZ"));
	languages.push_back (std::make_pair ("汉语/漢語", "zh_CN"));
	languages.push_back (std::make_pair ("Dansk", "da_DK"));
	languages.push_back (std::make_pair ("Deutsch", "de_DE"));
	languages.push_back (std::make_pair ("English", "en_GB"));
	languages.push_back (std::make_pair ("Español", "es_ES"));
	languages.push_back (std::make_pair ("Français", "fr_FR"));
	languages.push_back (std::make_pair ("Italiano", "it_IT"));
	languages.push_back (std::make_pair ("Nederlands", "nl_NL"));
	languages.push_back (std::make_pair ("Русский", "ru_RU"));
	languages.push_back (std::make_pair ("Polski", "pl_PL"));
	languages.push_back (std::make_pair ("Português europeu", "pt_PT"));
	languages.push_back (std::make_pair ("Português do Brasil", "pt_BR"));
	languages.push_back (std::make_pair ("Svenska", "sv_SE"));
	languages.push_back (std::make_pair ("Slovenský jazyk", "sk_SK"));
	languages.push_back (std::make_pair ("українська мова", "uk_UA"));
	checked_set (_language, languages);
	table->Add (_language, wxGBPosition (r, 1));
	++r;

	wxStaticText* restart = add_label_to_sizer (
		table, _panel, _("(restart DCP-o-matic to see language changes)"), false, wxGBPosition (r, 0), wxGBSpan (1, 2)
		);
	wxFont font = restart->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	restart->SetFont (font);
	++r;

	_set_language->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::set_language_changed, this));
	_language->Bind     (wxEVT_CHOICE,   boost::bind (&GeneralPage::language_changed,     this));
}

void
GeneralPage::add_play_sound_controls (wxGridBagSizer* table, int& r)
{
	_sound = new wxCheckBox (_panel, wxID_ANY, _("Play sound via"));
	table->Add (_sound, wxGBPosition (r, 0));
	_sound_output = new wxChoice (_panel, wxID_ANY);
	table->Add (_sound_output, wxGBPosition (r, 1));
	++r;

	RtAudio audio (DCPOMATIC_RTAUDIO_API);
	for (unsigned int i = 0; i < audio.getDeviceCount(); ++i) {
		RtAudio::DeviceInfo dev = audio.getDeviceInfo (i);
		if (dev.probed && dev.outputChannels > 0) {
			_sound_output->Append (std_to_wx (dev.name));
		}
	}

	_sound->Bind        (wxEVT_CHECKBOX, boost::bind (&GeneralPage::sound_changed, this));
	_sound_output->Bind (wxEVT_CHOICE,   boost::bind (&GeneralPage::sound_output_changed, this));
}

void
GeneralPage::add_update_controls (wxGridBagSizer* table, int& r)
{
	_check_for_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for updates on startup"));
	table->Add (_check_for_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_check_for_test_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for testing updates on startup"));
	table->Add (_check_for_test_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_check_for_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_updates_changed, this));
	_check_for_test_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_test_updates_changed, this));
}

void
GeneralPage::config_changed ()
{
	Config* config = Config::instance ();

	checked_set (_set_language, static_cast<bool>(config->language()));

	/* Backwards compatibility of config file */

	std::map<std::string, std::string> compat_map;
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

	std::string lang = config->language().get_value_or ("en_GB");
	if (compat_map.find (lang) != compat_map.end ()) {
		lang = compat_map[lang];
	}

	checked_set (_language, lang);

	checked_set (_check_for_updates, config->check_for_updates ());
	checked_set (_check_for_test_updates, config->check_for_test_updates ());

	checked_set (_sound, config->sound ());

	boost::optional<std::string> const current_so = get_sound_output ();
	boost::optional<std::string> configured_so;

	if (config->sound_output()) {
		configured_so = config->sound_output().get();
	} else {
		/* No configured output means we should use the default */
		RtAudio audio (DCPOMATIC_RTAUDIO_API);
		try {
			configured_so = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
		} catch (RtAudioError& e) {
			/* Probably no audio devices at all */
		}
	}

	if (configured_so && current_so != configured_so) {
		/* Update _sound_output with the configured value */
		unsigned int i = 0;
		while (i < _sound_output->GetCount()) {
			if (_sound_output->GetString(i) == std_to_wx(*configured_so)) {
				_sound_output->SetSelection (i);
				break;
			}
			++i;
		}
	}

	setup_sensitivity ();
}

void
GeneralPage::setup_sensitivity ()
{
	_language->Enable (_set_language->GetValue ());
	_check_for_test_updates->Enable (_check_for_updates->GetValue ());
	_sound_output->Enable (_sound->GetValue ());
}

/** @return Currently-selected preview sound output in the dialogue */
boost::optional<std::string>
GeneralPage::get_sound_output ()
{
	int const sel = _sound_output->GetSelection ();
	if (sel == wxNOT_FOUND) {
		return boost::optional<std::string> ();
	}

	return wx_to_std (_sound_output->GetString (sel));
}

void
GeneralPage::set_language_changed ()
{
	setup_sensitivity ();
	if (_set_language->GetValue ()) {
		language_changed ();
	} else {
		Config::instance()->unset_language ();
	}
}

void
GeneralPage::language_changed ()
{
	int const sel = _language->GetSelection ();
	if (sel != -1) {
		Config::instance()->set_language (string_client_data (_language->GetClientObject (sel)));
	} else {
		Config::instance()->unset_language ();
	}
}

void
GeneralPage::check_for_updates_changed ()
{
	Config::instance()->set_check_for_updates (_check_for_updates->GetValue ());
}

void
GeneralPage::check_for_test_updates_changed ()
{
	Config::instance()->set_check_for_test_updates (_check_for_test_updates->GetValue ());
}

void
GeneralPage::sound_changed ()
{
	Config::instance()->set_sound (_sound->GetValue ());
}

void
GeneralPage::sound_output_changed ()
{
	RtAudio audio (DCPOMATIC_RTAUDIO_API);
	boost::optional<std::string> const so = get_sound_output();
	if (!so || *so == audio.getDeviceInfo(audio.getDefaultOutputDevice()).name) {
		Config::instance()->unset_sound_output ();
	} else {
		Config::instance()->set_sound_output (*so);
	}
}

CertificateChainEditor::CertificateChainEditor (
	wxWindow* parent,
	wxString title,
	int border,
	boost::function<void (boost::shared_ptr<dcp::CertificateChain>)> set,
	boost::function<boost::shared_ptr<const dcp::CertificateChain> (void)> get,
	boost::function<void (void)> nag_remake
	)
	: wxDialog (parent, wxID_ANY, title)
	, _set (set)
	, _get (get)
	, _nag_remake (nag_remake)
{
	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	_sizer = new wxBoxSizer (wxVERTICAL);

	{
		wxStaticText* m = new wxStaticText (this, wxID_ANY, title);
		m->SetFont (subheading_font);
		_sizer->Add (m, 0, wxALL, border);
	}

	wxBoxSizer* certificates_sizer = new wxBoxSizer (wxHORIZONTAL);
	_sizer->Add (certificates_sizer, 0, wxLEFT | wxRIGHT, border);

	_certificates = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (440, 150), wxLC_REPORT | wxLC_SINGLE_SEL);

	{
		wxListItem ip;
		ip.SetId (0);
		ip.SetText (_("Type"));
		ip.SetWidth (100);
		_certificates->InsertColumn (0, ip);
	}

	{
		wxListItem ip;
		ip.SetId (1);
		ip.SetText (_("Thumbprint"));
		ip.SetWidth (340);

		wxFont font = ip.GetFont ();
		font.SetFamily (wxFONTFAMILY_TELETYPE);
		ip.SetFont (font);

		_certificates->InsertColumn (1, ip);
	}

	certificates_sizer->Add (_certificates, 1, wxEXPAND);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		_add_certificate = new wxButton (this, wxID_ANY, _("Add..."));
		s->Add (_add_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		_remove_certificate = new wxButton (this, wxID_ANY, _("Remove"));
		s->Add (_remove_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		_export_certificate = new wxButton (this, wxID_ANY, _("Export"));
		s->Add (_export_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		certificates_sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (table, 1, wxALL | wxEXPAND, border);
	int r = 0;

	add_label_to_sizer (table, this, _("Leaf private key"), true, wxGBPosition (r, 0));
	_private_key = new wxStaticText (this, wxID_ANY, wxT (""));
	wxFont font = _private_key->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	_private_key->SetFont (font);
	table->Add (_private_key, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_import_private_key = new wxButton (this, wxID_ANY, _("Import..."));
	table->Add (_import_private_key, wxGBPosition (r, 2));
	_export_private_key = new wxButton (this, wxID_ANY, _("Export..."));
	table->Add (_export_private_key, wxGBPosition (r, 3));
	++r;

	_button_sizer = new wxBoxSizer (wxHORIZONTAL);
	_remake_certificates = new wxButton (this, wxID_ANY, _("Re-make certificates and key..."));
	_button_sizer->Add (_remake_certificates, 1, wxRIGHT, border);
	table->Add (_button_sizer, wxGBPosition (r, 0), wxGBSpan (1, 4));
	++r;

	_private_key_bad = new wxStaticText (this, wxID_ANY, _("Leaf private key does not match leaf certificate!"));
	font = *wxSMALL_FONT;
	font.SetWeight (wxFONTWEIGHT_BOLD);
	_private_key_bad->SetFont (font);
	table->Add (_private_key_bad, wxGBPosition (r, 0), wxGBSpan (1, 3));
	++r;

	_add_certificate->Bind     (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::add_certificate, this));
	_remove_certificate->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::remove_certificate, this));
	_export_certificate->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::export_certificate, this));
	_certificates->Bind        (wxEVT_LIST_ITEM_SELECTED,   boost::bind (&CertificateChainEditor::update_sensitivity, this));
	_certificates->Bind        (wxEVT_LIST_ITEM_DESELECTED, boost::bind (&CertificateChainEditor::update_sensitivity, this));
	_remake_certificates->Bind (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::remake_certificates, this));
	_import_private_key->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::import_private_key, this));
	_export_private_key->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::export_private_key, this));

	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (_sizer);

	update_certificate_list ();
	update_private_key ();
	update_sensitivity ();
}

void
CertificateChainEditor::add_button (wxWindow* button)
{
	_button_sizer->Add (button, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_sizer->Layout ();
}

void
CertificateChainEditor::add_certificate ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));

	if (d->ShowModal() == wxID_OK) {
		try {
			dcp::Certificate c;
			std::string extra;
			try {
				extra = c.read_string (dcp::file_to_string (wx_to_std (d->GetPath ())));
			} catch (boost::filesystem::filesystem_error& e) {
				error_dialog (this, wxString::Format (_("Could not import certificate (%s)"), d->GetPath().data()));
				d->Destroy ();
				return;
			}

			if (!extra.empty ()) {
				message_dialog (
					this,
					_("This file contains other certificates (or other data) after its first certificate. "
					  "Only the first certificate will be used.")
					);
			}
			shared_ptr<dcp::CertificateChain> chain(new dcp::CertificateChain(*_get().get()));
			chain->add (c);
			if (!chain->chain_valid ()) {
				error_dialog (
					this,
					_("Adding this certificate would make the chain inconsistent, so it will not be added. "
					  "Add certificates in order from root to intermediate to leaf.")
					);
				chain->remove (c);
			} else {
				_set (chain);
				update_certificate_list ();
			}
		} catch (dcp::MiscError& e) {
			error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), e.what ()));
		}
	}

	d->Destroy ();

	update_sensitivity ();
}

void
CertificateChainEditor::remove_certificate ()
{
	int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	_certificates->DeleteItem (i);
	shared_ptr<dcp::CertificateChain> chain(new dcp::CertificateChain(*_get().get()));
	chain->remove (i);
	_set (chain);

	update_sensitivity ();
}

void
CertificateChainEditor::export_certificate ()
{
	int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	wxFileDialog* d = new wxFileDialog (
		this, _("Select Certificate File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	dcp::CertificateChain::List all = _get()->root_to_leaf ();
	dcp::CertificateChain::List::iterator j = all.begin ();
	for (int k = 0; k < i; ++k) {
		++j;
	}

	if (d->ShowModal () == wxID_OK) {
		FILE* f = fopen_boost (path_from_file_dialog (d, "pem"), "w");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		std::string const s = j->certificate (true);
		fwrite (s.c_str(), 1, s.length(), f);
		fclose (f);
	}
	d->Destroy ();
}

void
CertificateChainEditor::update_certificate_list ()
{
	_certificates->DeleteAllItems ();
	size_t n = 0;
	dcp::CertificateChain::List certs = _get()->root_to_leaf ();
	BOOST_FOREACH (dcp::Certificate const & i, certs) {
		wxListItem item;
		item.SetId (n);
		_certificates->InsertItem (item);
		_certificates->SetItem (n, 1, std_to_wx (i.thumbprint ()));

		if (n == 0) {
			_certificates->SetItem (n, 0, _("Root"));
		} else if (n == (certs.size() - 1)) {
			_certificates->SetItem (n, 0, _("Leaf"));
		} else {
			_certificates->SetItem (n, 0, _("Intermediate"));
		}

		++n;
	}

	static wxColour normal = _private_key_bad->GetForegroundColour ();

	if (_get()->private_key_valid()) {
		_private_key_bad->Hide ();
		_private_key_bad->SetForegroundColour (normal);
	} else {
		_private_key_bad->Show ();
		_private_key_bad->SetForegroundColour (wxColour (255, 0, 0));
	}
}

void
CertificateChainEditor::remake_certificates ()
{
	boost::shared_ptr<const dcp::CertificateChain> chain = _get();

	std::string subject_organization_name;
	std::string subject_organizational_unit_name;
	std::string root_common_name;
	std::string intermediate_common_name;
	std::string leaf_common_name;

	dcp::CertificateChain::List all = chain->root_to_leaf ();

	if (all.size() >= 1) {
		/* Have a root */
		subject_organization_name = chain->root().subject_organization_name ();
		subject_organizational_unit_name = chain->root().subject_organizational_unit_name ();
		root_common_name = chain->root().subject_common_name ();
	}

	if (all.size() >= 2) {
		/* Have a leaf */
		leaf_common_name = chain->leaf().subject_common_name ();
	}

	if (all.size() >= 3) {
		/* Have an intermediate */
		dcp::CertificateChain::List::iterator i = all.begin ();
		++i;
		intermediate_common_name = i->subject_common_name ();
	}

	_nag_remake ();

	MakeChainDialog* d = new MakeChainDialog (
		this,
		subject_organization_name,
		subject_organizational_unit_name,
		root_common_name,
		intermediate_common_name,
		leaf_common_name
		);

	if (d->ShowModal () == wxID_OK) {
		_set (
			shared_ptr<dcp::CertificateChain> (
				new dcp::CertificateChain (
					openssl_path (),
					d->organisation (),
					d->organisational_unit (),
					d->root_common_name (),
					d->intermediate_common_name (),
					d->leaf_common_name ()
					)
				)
			);

		update_certificate_list ();
		update_private_key ();
	}

	d->Destroy ();
}

void
CertificateChainEditor::update_sensitivity ()
{
	/* We can only remove the leaf certificate */
	_remove_certificate->Enable (_certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) == (_certificates->GetItemCount() - 1));
	_export_certificate->Enable (_certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
}

void
CertificateChainEditor::update_private_key ()
{
	checked_set (_private_key, dcp::private_key_fingerprint (_get()->key().get()));
	_sizer->Layout ();
}

void
CertificateChainEditor::import_private_key ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Key File"));

	if (d->ShowModal() == wxID_OK) {
		try {
			boost::filesystem::path p (wx_to_std (d->GetPath ()));
			if (boost::filesystem::file_size (p) > 8192) {
				error_dialog (
					this,
					wxString::Format (_("Could not read key file; file is too long (%s)"), std_to_wx (p.string ()))
					);
				return;
			}

			shared_ptr<dcp::CertificateChain> chain(new dcp::CertificateChain(*_get().get()));
			chain->set_key (dcp::file_to_string (p));
			_set (chain);
			update_private_key ();
		} catch (dcp::MiscError& e) {
			error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), e.what ()));
		}
	}

	d->Destroy ();

	update_sensitivity ();
}

void
CertificateChainEditor::export_private_key ()
{
	boost::optional<std::string> key = _get()->key();
	if (!key) {
		return;
	}

	wxFileDialog* d = new wxFileDialog (
		this, _("Select Key File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal () == wxID_OK) {
		FILE* f = fopen_boost (path_from_file_dialog (d, "pem"), "w");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		std::string const s = _get()->key().get ();
		fwrite (s.c_str(), 1, s.length(), f);
		fclose (f);
	}
	d->Destroy ();
}

wxString
KeysPage::GetName () const
{
	return _("Keys");
}

void
KeysPage::setup ()
{
	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	wxSizer* sizer = _panel->GetSizer();

	{
		wxStaticText* m = new wxStaticText (_panel, wxID_ANY, _("Decrypting KDMs"));
		m->SetFont (subheading_font);
		sizer->Add (m, 0, wxALL, _border);
	}

	wxButton* export_decryption_certificate = new wxButton (_panel, wxID_ANY, _("Export KDM decryption certificate..."));
	sizer->Add (export_decryption_certificate, 0, wxLEFT, _border);
	wxButton* export_decryption_chain = new wxButton (_panel, wxID_ANY, _("Export KDM decryption chain..."));
	sizer->Add (export_decryption_chain, 0, wxLEFT, _border);
	wxButton* export_settings = new wxButton (_panel, wxID_ANY, _("Export all KDM decryption settings..."));
	sizer->Add (export_settings, 0, wxLEFT, _border);
	wxButton* import_settings = new wxButton (_panel, wxID_ANY, _("Import all KDM decryption settings..."));
	sizer->Add (import_settings, 0, wxLEFT, _border);
	wxButton* decryption_advanced = new wxButton (_panel, wxID_ANY, _("Advanced..."));
	sizer->Add (decryption_advanced, 0, wxALL, _border);

	export_decryption_certificate->Bind (wxEVT_BUTTON, bind (&KeysPage::export_decryption_certificate, this));
	export_decryption_chain->Bind (wxEVT_BUTTON, bind (&KeysPage::export_decryption_chain, this));
	export_settings->Bind (wxEVT_BUTTON, bind (&KeysPage::export_decryption_chain_and_key, this));
	import_settings->Bind (wxEVT_BUTTON, bind (&KeysPage::import_decryption_chain_and_key, this));
	decryption_advanced->Bind (wxEVT_BUTTON, bind (&KeysPage::decryption_advanced, this));

	{
		wxStaticText* m = new wxStaticText (_panel, wxID_ANY, _("Signing DCPs and KDMs"));
		m->SetFont (subheading_font);
		sizer->Add (m, 0, wxALL, _border);
	}

	wxButton* signing_advanced = new wxButton (_panel, wxID_ANY, _("Advanced..."));
	sizer->Add (signing_advanced, 0, wxLEFT, _border);
	signing_advanced->Bind (wxEVT_BUTTON, bind (&KeysPage::signing_advanced, this));
}

void
KeysPage::decryption_advanced ()
{
	CertificateChainEditor* c = new CertificateChainEditor (
		_panel, _("Decrypting KDMs"), _border,
		bind (&Config::set_decryption_chain, Config::instance (), _1),
		bind (&Config::decryption_chain, Config::instance ()),
		bind (&KeysPage::nag_remake_decryption_chain, this)
		);

	c->ShowModal();
}

void
KeysPage::signing_advanced ()
{
	CertificateChainEditor* c = new CertificateChainEditor (
		_panel, _("Signing DCPs and KDMs"), _border,
		bind (&Config::set_signer_chain, Config::instance (), _1),
		bind (&Config::signer_chain, Config::instance ()),
		bind (&do_nothing)
		);

	c->ShowModal();
}

void
KeysPage::export_decryption_chain_and_key ()
{
	wxFileDialog* d = new wxFileDialog (
		_panel, _("Select Export File"), wxEmptyString, wxEmptyString, wxT ("DOM files (*.dom)|*.dom"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal () == wxID_OK) {
		FILE* f = fopen_boost (path_from_file_dialog (d, "dom"), "w");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		string const chain = Config::instance()->decryption_chain()->chain();
		fwrite (chain.c_str(), 1, chain.length(), f);
		optional<string> const key = Config::instance()->decryption_chain()->key();
		DCPOMATIC_ASSERT (key);
		fwrite (key->c_str(), 1, key->length(), f);
		fclose (f);
	}
	d->Destroy ();

}

void
KeysPage::import_decryption_chain_and_key ()
{
	wxFileDialog* d = new wxFileDialog (
		_panel, _("Select File To Import"), wxEmptyString, wxEmptyString, wxT ("DOM files (*.dom)|*.dom")
		);

	if (d->ShowModal () == wxID_OK) {
		shared_ptr<dcp::CertificateChain> new_chain(new dcp::CertificateChain());

		FILE* f = fopen_boost (wx_to_std (d->GetPath ()), "r");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		string current;
		while (!feof (f)) {
			char buffer[128];
			if (fgets (buffer, 128, f) == 0) {
				break;
			}
			current += buffer;
			if (strncmp (buffer, "-----END CERTIFICATE-----", 25) == 0) {
				new_chain->add (dcp::Certificate (current));
				current = "";
			} else if (strncmp (buffer, "-----END RSA PRIVATE KEY-----", 29) == 0) {
				std::cout << "the key is " << current << "\n";
				new_chain->set_key (current);
				current = "";
			}
		}
		fclose (f);

		if (new_chain->chain_valid() && new_chain->private_key_valid()) {
			Config::instance()->set_decryption_chain (new_chain);
		} else {
			error_dialog (_panel, _("Invalid DCP-o-matic export file"));
		}
	}
	d->Destroy ();
}

void
KeysPage::nag_remake_decryption_chain ()
{
	NagDialog::maybe_nag (
		_panel,
		Config::NAG_REMAKE_DECRYPTION_CHAIN,
		_("If you continue with this operation you will no longer be able to use any DKDMs that you have created.  Also, any KDMs that have been sent to you will become useless.  Proceed with caution!")
		);
}

void
KeysPage::export_decryption_chain ()
{
	wxFileDialog* d = new wxFileDialog (
		_panel, _("Select Chain File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal () == wxID_OK) {
		FILE* f = fopen_boost (path_from_file_dialog (d, "pem"), "w");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		std::string const s = Config::instance()->decryption_chain()->chain();
		fwrite (s.c_str(), 1, s.length(), f);
		fclose (f);
	}
	d->Destroy ();
}

void
KeysPage::export_decryption_certificate ()
{
	wxFileDialog* d = new wxFileDialog (
		_panel, _("Select Certificate File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal () == wxID_OK) {
		FILE* f = fopen_boost (path_from_file_dialog (d, "pem"), "w");
		if (!f) {
			throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
		}

		std::string const s = Config::instance()->decryption_chain()->leaf().certificate (true);
		fwrite (s.c_str(), 1, s.length(), f);
		fclose (f);
	}
	d->Destroy ();
}
