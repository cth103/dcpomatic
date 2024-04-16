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


#include "audio_mapping_view.h"
#include "check_box.h"
#include "config_dialog.h"
#include "dcpomatic_button.h"
#include "nag_dialog.h"
#include "static_text.h"
#include "wx_variant.h"
#include "lib/constants.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <dcp/raw_convert.h>


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


static
bool
do_nothing ()
{
	return false;
}

Page::Page (wxSize panel_size, int border)
	: _border (border)
	, _panel (0)
	, _panel_size (panel_size)
	, _window_exists (false)
{
	_config_connection = Config::instance()->Changed.connect (bind (&Page::config_changed_wrapper, this));
}


wxWindow*
Page::CreateWindow (wxWindow* parent)
{
	return create_window (parent);
}


wxWindow*
Page::create_window (wxWindow* parent)
{
	_panel = new wxPanel (parent, wxID_ANY, wxDefaultPosition, _panel_size);
	auto s = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (s);

	setup ();
	_window_exists = true;
	config_changed ();

	_panel->Bind (wxEVT_DESTROY, bind (&Page::window_destroyed, this));

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


GeneralPage::GeneralPage (wxSize panel_size, int border)
	: Page (panel_size, border)
{

}


wxString
GeneralPage::GetName () const
{
	return _("General");
}


void
GeneralPage::add_language_controls (wxGridBagSizer* table, int& r)
{
	_set_language = new CheckBox (_panel, _("Set language"));
	table->Add (_set_language, wxGBPosition (r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_language = new wxChoice (_panel, wxID_ANY);
	vector<pair<string, string>> languages;
	languages.push_back (make_pair("Čeština", "cs_CZ"));
	languages.push_back (make_pair("汉语/漢語", "zh_CN"));
	languages.push_back (make_pair("Dansk", "da_DK"));
	languages.push_back (make_pair("Deutsch", "de_DE"));
	languages.push_back (make_pair("English", "en_GB"));
	languages.push_back (make_pair("Español", "es_ES"));
	languages.push_back (make_pair("فارسی", "fa_IR"));
	languages.push_back (make_pair("Français", "fr_FR"));
	languages.push_back (make_pair("Italiano", "it_IT"));
	languages.push_back (make_pair("ქართული", "ka_KA"));
	languages.push_back (make_pair("Nederlands", "nl_NL"));
	languages.push_back (make_pair("Русский", "ru_RU"));
	languages.push_back (make_pair("Polski", "pl_PL"));
	languages.push_back (make_pair("Português europeu", "pt_PT"));
	languages.push_back (make_pair("Português do Brasil", "pt_BR"));
	languages.push_back (make_pair("Svenska", "sv_SE"));
	languages.push_back (make_pair("Slovenščina", "sl_SI"));
	languages.push_back (make_pair("Slovenský jazyk", "sk_SK"));
	// languages.push_back (make_pair("Türkçe", "tr_TR"));
	languages.push_back (make_pair("українська мова", "uk_UA"));
	languages.push_back (make_pair("Magyar nyelv", "hu_HU"));
	checked_set (_language, languages);
	table->Add (_language, wxGBPosition (r, 1));
	++r;

	auto restart = add_label_to_sizer (
		table, _panel, variant::wx::insert_dcpomatic(_("(restart %s to see language changes)")), false, wxGBPosition (r, 0), wxGBSpan (1, 2)
		);
	wxFont font = restart->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	restart->SetFont (font);
	++r;

	_set_language->bind(&GeneralPage::set_language_changed, this);
	_language->Bind     (wxEVT_CHOICE,   bind (&GeneralPage::language_changed,     this));
}

void
GeneralPage::add_update_controls (wxGridBagSizer* table, int& r)
{
	_check_for_updates = new CheckBox (_panel, _("Check for updates on startup"));
	table->Add (_check_for_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_check_for_test_updates = new CheckBox (_panel, _("Check for testing updates on startup"));
	table->Add (_check_for_test_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_check_for_updates->bind(&GeneralPage::check_for_updates_changed, this);
	_check_for_test_updates->bind(&GeneralPage::check_for_test_updates_changed, this);
}

void
GeneralPage::config_changed ()
{
	auto config = Config::instance ();

	checked_set (_set_language, static_cast<bool>(config->language()));

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
	if (compat_map.find(lang) != compat_map.end ()) {
		lang = compat_map[lang];
	}

	checked_set (_language, lang);

	checked_set (_check_for_updates, config->check_for_updates ());
	checked_set (_check_for_test_updates, config->check_for_test_updates ());

	setup_sensitivity ();
}

void
GeneralPage::setup_sensitivity ()
{
	_language->Enable (_set_language->GetValue ());
	_check_for_test_updates->Enable (_check_for_updates->GetValue ());
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

CertificateChainEditor::CertificateChainEditor (
	wxWindow* parent,
	wxString title,
	int border,
	function<void (shared_ptr<dcp::CertificateChain>)> set,
	function<shared_ptr<const dcp::CertificateChain> (void)> get,
	function<bool (void)> nag_alter
	)
	: wxDialog (parent, wxID_ANY, title)
	, _set (set)
	, _get (get)
	, _nag_alter (nag_alter)
{
	_sizer = new wxBoxSizer (wxVERTICAL);

	auto certificates_sizer = new wxBoxSizer (wxHORIZONTAL);
	_sizer->Add (certificates_sizer, 0, wxALL, border);

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
		auto s = new wxBoxSizer (wxVERTICAL);
		_add_certificate = new Button (this, _("Add..."));
		s->Add (_add_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_remove_certificate = new Button (this, _("Remove"));
		s->Add (_remove_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_export_certificate = new Button (this, _("Export certificate..."));
		s->Add (_export_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_export_chain = new Button (this, _("Export chain..."));
		s->Add (_export_chain, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		certificates_sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	auto table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (table, 1, wxALL | wxEXPAND, border);
	int r = 0;

	add_label_to_sizer (table, this, _("Leaf private key"), true, wxGBPosition (r, 0));
	_private_key = new StaticText (this, wxT(""));
	wxFont font = _private_key->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	_private_key->SetFont (font);
	table->Add (_private_key, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_import_private_key = new Button (this, _("Import..."));
	table->Add (_import_private_key, wxGBPosition (r, 2));
	_export_private_key = new Button (this, _("Export..."));
	table->Add (_export_private_key, wxGBPosition (r, 3));
	++r;

	_button_sizer = new wxBoxSizer (wxHORIZONTAL);
	_remake_certificates = new Button (this, _("Re-make certificates and key..."));
	_button_sizer->Add (_remake_certificates, 1, wxRIGHT, border);
	table->Add (_button_sizer, wxGBPosition (r, 0), wxGBSpan (1, 4));
	++r;

	_private_key_bad = new StaticText (this, _("Leaf private key does not match leaf certificate!"));
	font = *wxSMALL_FONT;
	font.SetWeight (wxFONTWEIGHT_BOLD);
	_private_key_bad->SetFont (font);
	table->Add (_private_key_bad, wxGBPosition (r, 0), wxGBSpan (1, 3));
	++r;

	_add_certificate->Bind     (wxEVT_BUTTON,       bind (&CertificateChainEditor::add_certificate, this));
	_remove_certificate->Bind  (wxEVT_BUTTON,       bind (&CertificateChainEditor::remove_certificate, this));
	_export_certificate->Bind  (wxEVT_BUTTON,       bind (&CertificateChainEditor::export_certificate, this));
	_certificates->Bind        (wxEVT_LIST_ITEM_SELECTED,   bind (&CertificateChainEditor::update_sensitivity, this));
	_certificates->Bind        (wxEVT_LIST_ITEM_DESELECTED, bind (&CertificateChainEditor::update_sensitivity, this));
	_remake_certificates->Bind (wxEVT_BUTTON,       bind (&CertificateChainEditor::remake_certificates, this));
	_export_chain->Bind        (wxEVT_BUTTON,       bind (&CertificateChainEditor::export_chain, this));
	_import_private_key->Bind  (wxEVT_BUTTON,       bind (&CertificateChainEditor::import_private_key, this));
	_export_private_key->Bind  (wxEVT_BUTTON,       bind (&CertificateChainEditor::export_private_key, this));

	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
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
	auto d = make_wx<wxFileDialog>(this, _("Select Certificate File"));

	if (d->ShowModal() == wxID_OK) {
		try {
			dcp::Certificate c;
			string extra;
			try {
				extra = c.read_string (dcp::file_to_string (wx_to_std (d->GetPath ())));
			} catch (boost::filesystem::filesystem_error& e) {
				error_dialog (this, _("Could not import certificate (%s)"), d->GetPath());
				return;
			}

			if (!extra.empty ()) {
				message_dialog (
					this,
					_("This file contains other certificates (or other data) after its first certificate. "
					  "Only the first certificate will be used.")
					);
			}
			auto chain = make_shared<dcp::CertificateChain>(*_get().get());
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
			error_dialog (this, _("Could not read certificate file."), std_to_wx(e.what()));
		}
	}

	update_sensitivity ();
}

void
CertificateChainEditor::remove_certificate ()
{
	if (_nag_alter()) {
		/* Cancel was clicked */
		return;
	}

	int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	_certificates->DeleteItem (i);
	auto chain = make_shared<dcp::CertificateChain>(*_get().get());
	chain->remove (i);
	_set (chain);

	update_sensitivity ();
	update_certificate_list ();
}

void
CertificateChainEditor::export_certificate ()
{
	int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	auto all = _get()->root_to_leaf();

	wxString default_name;
	if (i == 0) {
		default_name = "root.pem";
	} else if (i == static_cast<int>(all.size() - 1)) {
		default_name = "leaf.pem";
	} else {
		default_name = "intermediate.pem";
	}

	auto d = make_wx<wxFileDialog>(
		this, _("Select Certificate File"), wxEmptyString, default_name, wxT ("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	auto j = all.begin ();
	for (int k = 0; k < i; ++k) {
		++j;
	}

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(d->GetPath()));
	if (path.extension() != ".pem") {
		path += ".pem";
	}
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, errno, OpenFileError::WRITE);
	}

	string const s = j->certificate(true);
	f.checked_write(s.c_str(), s.length());
}

void
CertificateChainEditor::export_chain ()
{
	auto d = make_wx<wxFileDialog>(
		this, _("Select Chain File"), wxEmptyString, wxT("certificate_chain.pem"), wxT("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(d->GetPath()));
	if (path.extension() != ".pem") {
		path += ".pem";
	}
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, errno, OpenFileError::WRITE);
	}

	auto const s = _get()->chain();
	f.checked_write(s.c_str(), s.length());
}

void
CertificateChainEditor::update_certificate_list ()
{
	_certificates->DeleteAllItems ();
	size_t n = 0;
	auto certs = _get()->root_to_leaf();
	for (auto const& i: certs) {
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
	if (_nag_alter()) {
		/* Cancel was clicked */
		return;
	}

	auto d = make_wx<MakeChainDialog>(this, _get());

	if (d->ShowModal () == wxID_OK) {
		_set (d->get());
		update_certificate_list ();
		update_private_key ();
	}
}

void
CertificateChainEditor::update_sensitivity ()
{
	/* We can only remove the leaf certificate */
	_remove_certificate->Enable (_certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) == (_certificates->GetItemCount() - 1));
	_export_certificate->Enable (_certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
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
	auto d = make_wx<wxFileDialog>(this, _("Select Key File"));

	if (d->ShowModal() == wxID_OK) {
		try {
			boost::filesystem::path p (wx_to_std (d->GetPath ()));
			if (dcp::filesystem::file_size(p) > 8192) {
				error_dialog (
					this,
					wxString::Format (_("Could not read key file; file is too long (%s)"), std_to_wx (p.string ()))
					);
				return;
			}

			auto chain = make_shared<dcp::CertificateChain>(*_get().get());
			chain->set_key (dcp::file_to_string (p));
			_set (chain);
			update_private_key ();
		} catch (std::exception& e) {
			error_dialog (this, _("Could not read certificate file."), std_to_wx(e.what()));
		}
	}

	update_sensitivity ();
}

void
CertificateChainEditor::export_private_key ()
{
	auto key = _get()->key();
	if (!key) {
		return;
	}

	auto d = make_wx<wxFileDialog>(
		this, _("Select Key File"), wxEmptyString, wxT("private_key.pem"), wxT("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal () == wxID_OK) {
		boost::filesystem::path path (wx_to_std(d->GetPath()));
		if (path.extension() != ".pem") {
			path += ".pem";
		}
		dcp::File f(path, "w");
		if (!f) {
			throw OpenFileError (path, errno, OpenFileError::WRITE);
		}

		auto const s = _get()->key().get ();
		f.checked_write(s.c_str(), s.length());
	}
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

	auto sizer = _panel->GetSizer();

	{
		auto m = new StaticText (_panel, _("Decrypting KDMs"));
		m->SetFont (subheading_font);
		sizer->Add (m, 0, wxALL | wxEXPAND, _border);
	}

	auto kdm_buttons = new wxBoxSizer (wxVERTICAL);

	auto export_decryption_certificate = new Button (_panel, _("Export KDM decryption leaf certificate..."));
	kdm_buttons->Add (export_decryption_certificate, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto export_settings = new Button (_panel, _("Export all KDM decryption settings..."));
	kdm_buttons->Add (export_settings, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto import_settings = new Button (_panel, _("Import all KDM decryption settings..."));
	kdm_buttons->Add (import_settings, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto decryption_advanced = new Button (_panel, _("Advanced..."));
	kdm_buttons->Add (decryption_advanced, 0);

	sizer->Add (kdm_buttons, 0, wxLEFT, _border);

	export_decryption_certificate->Bind (wxEVT_BUTTON, bind (&KeysPage::export_decryption_certificate, this));
	export_settings->Bind (wxEVT_BUTTON, bind (&KeysPage::export_decryption_chain_and_key, this));
	import_settings->Bind (wxEVT_BUTTON, bind (&KeysPage::import_decryption_chain_and_key, this));
	decryption_advanced->Bind (wxEVT_BUTTON, bind (&KeysPage::decryption_advanced, this));

	{
		auto m = new StaticText (_panel, _("Signing DCPs and KDMs"));
		m->SetFont (subheading_font);
		sizer->Add (m, 0, wxALL | wxEXPAND, _border);
	}

	auto signing_buttons = new wxBoxSizer (wxVERTICAL);

	auto signing_advanced = new Button (_panel, _("Advanced..."));
	signing_buttons->Add (signing_advanced, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	auto remake_signing = new Button (_panel, _("Re-make certificates and key..."));
	signing_buttons->Add (remake_signing, 0, wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	sizer->Add (signing_buttons, 0, wxLEFT, _border);

	signing_advanced->Bind (wxEVT_BUTTON, bind (&KeysPage::signing_advanced, this));
	remake_signing->Bind (wxEVT_BUTTON, bind(&KeysPage::remake_signing, this));
}


void
KeysPage::remake_signing ()
{
	auto d = new MakeChainDialog (_panel, Config::instance()->signer_chain());

	if (d->ShowModal () == wxID_OK) {
		Config::instance()->set_signer_chain(d->get());
	}
}


void
KeysPage::decryption_advanced ()
{
	auto c = new CertificateChainEditor (
		_panel, _("Decrypting KDMs"), _border,
		bind(&Config::set_decryption_chain, Config::instance(), _1),
		bind(&Config::decryption_chain, Config::instance()),
		bind(&KeysPage::nag_alter_decryption_chain, this)
		);

	c->ShowModal();
}

void
KeysPage::signing_advanced ()
{
	auto c = new CertificateChainEditor (
		_panel, _("Signing DCPs and KDMs"), _border,
		bind(&Config::set_signer_chain, Config::instance(), _1),
		bind(&Config::signer_chain, Config::instance()),
		bind(&do_nothing)
		);

	c->ShowModal();
}

void
KeysPage::export_decryption_chain_and_key ()
{
	auto d = make_wx<wxFileDialog>(
		_panel, _("Select Export File"), wxEmptyString, wxEmptyString, wxT ("DOM files (*.dom)|*.dom"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(d->GetPath()));
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, errno, OpenFileError::WRITE);
	}

	auto const chain = Config::instance()->decryption_chain()->chain();
	f.checked_write(chain.c_str(), chain.length());
	auto const key = Config::instance()->decryption_chain()->key();
	DCPOMATIC_ASSERT(key);
	f.checked_write(key->c_str(), key->length());
}

void
KeysPage::import_decryption_chain_and_key ()
{
	if (NagDialog::maybe_nag (
		    _panel,
		    Config::NAG_IMPORT_DECRYPTION_CHAIN,
		    _("If you continue with this operation you will no longer be able to use any DKDMs that you have created with the current certificates and key.  Also, any KDMs that have been sent to you for those certificates will become useless.  Proceed with caution!"),
		    true
		    )) {
		return;
	}

	auto d = make_wx<wxFileDialog>(
		_panel, _("Select File To Import"), wxEmptyString, wxEmptyString, wxT ("DOM files (*.dom)|*.dom")
		);

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	auto new_chain = make_shared<dcp::CertificateChain>();

	dcp::File f(wx_to_std(d->GetPath()), "r");
	if (!f) {
		throw OpenFileError(f.path(), errno, OpenFileError::WRITE);
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
		} else if (strncmp (buffer, "-----END RSA PRIVATE KEY-----", 29) == 0) {
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
KeysPage::nag_alter_decryption_chain ()
{
	return NagDialog::maybe_nag (
		_panel,
		Config::NAG_ALTER_DECRYPTION_CHAIN,
		_("If you continue with this operation you will no longer be able to use any DKDMs that you have created.  Also, any KDMs that have been sent to you will become useless.  Proceed with caution!"),
		true
		);
}

void
KeysPage::export_decryption_certificate ()
{
	auto config = Config::instance();
	wxString default_name = "dcpomatic";
	if (!config->dcp_creator().empty()) {
		default_name += "_" + std_to_wx(careful_string_filter(config->dcp_creator()));
	}
	if (!config->dcp_issuer().empty()) {
		default_name += "_" + std_to_wx(careful_string_filter(config->dcp_issuer()));
	}
	default_name += wxT("_kdm_decryption_cert.pem");

	auto d = make_wx<wxFileDialog>(
		_panel, _("Select Certificate File"), wxEmptyString, default_name, wxT("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	boost::filesystem::path path(wx_to_std(d->GetPath()));
	if (path.extension() != ".pem") {
		path += ".pem";
	}
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, errno, OpenFileError::WRITE);
	}

	auto const s = Config::instance()->decryption_chain()->leaf().certificate (true);
	f.checked_write(s.c_str(), s.length());
}

wxString
SoundPage::GetName () const
{
	return _("Sound");
}

void
SoundPage::setup ()
{
	auto table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

	int r = 0;

	_sound = new CheckBox (_panel, _("Play sound via"));
	table->Add (_sound, wxGBPosition (r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_sound_output = new wxChoice (_panel, wxID_ANY);
	s->Add (_sound_output, 0);
	_sound_output_details = new wxStaticText (_panel, wxID_ANY, wxT(""));
	s->Add (_sound_output_details, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, DCPOMATIC_SIZER_X_GAP);
	table->Add (s, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (table, _panel, _("Mapping"), true, wxGBPosition(r, 0));
	_map = new AudioMappingView (_panel, _("DCP"), _("DCP"), _("Output"), _("output"));
	table->Add (_map, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	_reset_to_default = new Button (_panel, _("Reset to default"));
	table->Add (_reset_to_default, wxGBPosition(r, 1));
	++r;

	wxFont font = _sound_output_details->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	_sound_output_details->SetFont (font);

	RtAudio audio (DCPOMATIC_RTAUDIO_API);
#if (RTAUDIO_VERSION_MAJOR >= 6)
	for (auto device_id: audio.getDeviceIds()) {
		auto dev = audio.getDeviceInfo(device_id);
		if (dev.outputChannels > 0) {
			_sound_output->Append(std_to_wx(dev.name));
		}
	}
#else
	for (unsigned int i = 0; i < audio.getDeviceCount(); ++i) {
		try {
			auto dev = audio.getDeviceInfo (i);
			if (dev.probed && dev.outputChannels > 0) {
				_sound_output->Append (std_to_wx (dev.name));
			}
		} catch (RtAudioError&) {
			/* Something went wrong so let's just ignore that device */
		}
	}
#endif

	_sound->bind(&SoundPage::sound_changed, this);
	_sound_output->Bind (wxEVT_CHOICE,   bind(&SoundPage::sound_output_changed, this));
	_map->Changed.connect (bind(&SoundPage::map_changed, this, _1));
	_reset_to_default->Bind (wxEVT_BUTTON,   bind(&SoundPage::reset_to_default, this));
}

void
SoundPage::reset_to_default ()
{
	Config::instance()->set_audio_mapping_to_default ();
}

void
SoundPage::map_changed (AudioMapping m)
{
	Config::instance()->set_audio_mapping (m);
}

void
SoundPage::sound_changed ()
{
	Config::instance()->set_sound (_sound->GetValue ());
}

void
SoundPage::sound_output_changed ()
{
	RtAudio audio (DCPOMATIC_RTAUDIO_API);
	auto const so = get_sound_output();
	string default_device;
#if (RTAUDIO_VERSION_MAJOR >= 6)
	default_device = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
#else
	try {
		default_device = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
	} catch (RtAudioError&) {
		/* Never mind */
	}
#endif
	if (!so || *so == default_device) {
		Config::instance()->unset_sound_output ();
	} else {
		Config::instance()->set_sound_output (*so);
	}
}

void
SoundPage::config_changed ()
{
	auto config = Config::instance ();

	checked_set (_sound, config->sound ());

	auto const current_so = get_sound_output ();
	optional<string> configured_so;

	if (config->sound_output()) {
		configured_so = config->sound_output().get();
	} else {
		/* No configured output means we should use the default */
		RtAudio audio (DCPOMATIC_RTAUDIO_API);
#if (RTAUDIO_VERSION_MAJOR >= 6)
		configured_so = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
#else
		try {
			configured_so = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
		} catch (RtAudioError&) {
			/* Probably no audio devices at all */
		}
#endif
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

	RtAudio audio (DCPOMATIC_RTAUDIO_API);

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

	int channels = 0;
	if (configured_so) {
#if (RTAUDIO_VERSION_MAJOR >= 6)
		for (auto device_id: audio.getDeviceIds()) {
			auto info = audio.getDeviceInfo(device_id);
			if (info.name == *configured_so && info.outputChannels > 0) {
				channels = info.outputChannels;
			}
		}
#else
		for (unsigned int i = 0; i < audio.getDeviceCount(); ++i) {
			try {
				auto info = audio.getDeviceInfo(i);
				if (info.name == *configured_so && info.outputChannels > 0) {
					channels = info.outputChannels;
				}
			} catch (RtAudioError&) {
				/* Never mind */
			}
		}
#endif
	}

	_sound_output_details->SetLabel (
		wxString::Format(_("%d channels on %s"), channels, apis[audio.getCurrentApi()])
		);

	_map->set (Config::instance()->audio_mapping(channels));

	vector<NamedChannel> input;
	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		input.push_back (NamedChannel(short_audio_channel_name(i), i));
	}
	_map->set_input_channels (input);

	vector<NamedChannel> output;
	for (int i = 0; i < channels; ++i) {
		output.push_back (NamedChannel(dcp::raw_convert<string>(i), i));
	}
	_map->set_output_channels (output);

	setup_sensitivity ();
}

void
SoundPage::setup_sensitivity ()
{
	_sound_output->Enable (_sound->GetValue());
}

/** @return Currently-selected preview sound output in the dialogue */
optional<string>
SoundPage::get_sound_output ()
{
	int const sel = _sound_output->GetSelection ();
	if (sel == wxNOT_FOUND) {
		return optional<string> ();
	}

	return wx_to_std (_sound_output->GetString (sel));
}


LocationsPage::LocationsPage (wxSize panel_size, int border)
	: Page (panel_size, border)
{

}

wxString
LocationsPage::GetName () const
{
	return _("Locations");
}

#ifdef DCPOMATIC_OSX
wxBitmap
LocationsPage::GetLargeIcon () const
{
	return wxBitmap(icon_path("locations"), wxBITMAP_TYPE_PNG);
}
#endif

void
LocationsPage::setup ()
{
	int r = 0;

	auto table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

	add_label_to_sizer (table, _panel, _("Content directory"), true, wxGBPosition (r, 0));
	_content_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
	table->Add (_content_directory, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (table, _panel, _("Playlist directory"), true, wxGBPosition (r, 0));
	_playlist_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
	table->Add (_playlist_directory, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (table, _panel, _("KDM directory"), true, wxGBPosition (r, 0));
	_kdm_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
	table->Add (_kdm_directory, wxGBPosition (r, 1));
	++r;

	_content_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::content_directory_changed, this));
	_playlist_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::playlist_directory_changed, this));
	_kdm_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::kdm_directory_changed, this));
}

void
LocationsPage::config_changed ()
{
	auto config = Config::instance ();

	if (config->player_content_directory()) {
		checked_set (_content_directory, *config->player_content_directory());
	}
	if (config->player_playlist_directory()) {
		checked_set (_playlist_directory, *config->player_playlist_directory());
	}
	if (config->player_kdm_directory()) {
		checked_set (_kdm_directory, *config->player_kdm_directory());
	}
}

void
LocationsPage::content_directory_changed ()
{
	Config::instance()->set_player_content_directory(wx_to_std(_content_directory->GetPath()));
}

void
LocationsPage::playlist_directory_changed ()
{
	Config::instance()->set_player_playlist_directory(wx_to_std(_playlist_directory->GetPath()));
}

void
LocationsPage::kdm_directory_changed ()
{
	Config::instance()->set_player_kdm_directory(wx_to_std(_kdm_directory->GetPath()));
}
