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


#include "certificate_chain_editor.h"
#include "dcpomatic_button.h"
#include "make_chain_dialog.h"
#include "nag_dialog.h"
#include "keys_preferences_page.h"
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/exceptions.h"
#include "lib/export_decryption_settings.h"
#include "lib/util.h"
#include <dcp/file.h>



using std::make_shared;
using std::string;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;


KeysPage::KeysPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}


#ifdef DCPOMATIC_OSX
wxBitmap
KeysPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("keys"), wxBITMAP_TYPE_PNG);
}
#endif


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

	sizer->Add(signing_buttons, 0, wxLEFT | wxBOTTOM, _border);

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


static
bool
do_nothing()
{
	return false;
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

	::export_decryption_chain_and_key(Config::instance()->decryption_chain(), wx_to_std(dialog.GetPath()));
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

	if (auto new_chain = ::import_decryption_chain_and_key(wx_to_std(dialog.GetPath()))) {
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

