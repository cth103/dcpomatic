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
#include "static_text.h"
#include "wx_util.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <wx/listctrl.h>


using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


CertificateChainEditor::CertificateChainEditor(
	wxWindow* parent,
	wxString title,
	int border,
	function<void (shared_ptr<dcp::CertificateChain>)> set,
	function<shared_ptr<const dcp::CertificateChain> (void)> get,
	function<bool (void)> nag_alter
	)
	: wxDialog(parent, wxID_ANY, title)
	, _set(set)
	, _get(get)
	, _nag_alter(nag_alter)
{
	_sizer = new wxBoxSizer(wxVERTICAL);

	auto certificates_sizer = new wxBoxSizer(wxHORIZONTAL);
	_sizer->Add(certificates_sizer, 0, wxALL, border);

	_certificates = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(440, 150), wxLC_REPORT | wxLC_SINGLE_SEL);

	{
		wxListItem ip;
		ip.SetId(0);
		ip.SetText(_("Type"));
		ip.SetWidth(100);
		_certificates->InsertColumn(0, ip);
	}

	{
		wxListItem ip;
		ip.SetId(1);
		ip.SetText(_("Thumbprint"));
		ip.SetWidth(340);

		auto font = ip.GetFont();
		font.SetFamily(wxFONTFAMILY_TELETYPE);
		ip.SetFont(font);

		_certificates->InsertColumn(1, ip);
	}

	certificates_sizer->Add(_certificates, 1, wxEXPAND);

	{
		auto s = new wxBoxSizer(wxVERTICAL);
		_add_certificate = new Button(this, _("Add..."));
		s->Add(_add_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_remove_certificate = new Button(this, _("Remove"));
		s->Add(_remove_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_export_certificate = new Button(this, _("Export certificate..."));
		s->Add(_export_certificate, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_export_chain = new Button(this, _("Export chain..."));
		s->Add(_export_chain, 1, wxTOP | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		certificates_sizer->Add(s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add(table, 1, wxALL | wxEXPAND, border);
	int r = 0;

	add_label_to_sizer(table, this, _("Leaf private key"), true, wxGBPosition(r, 0));
	_private_key = new StaticText(this, {});
	wxFont font = _private_key->GetFont();
	font.SetFamily(wxFONTFAMILY_TELETYPE);
	_private_key->SetFont(font);
	table->Add(_private_key, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_import_private_key = new Button(this, _("Import..."));
	table->Add(_import_private_key, wxGBPosition(r, 2));
	_export_private_key = new Button(this, _("Export..."));
	table->Add(_export_private_key, wxGBPosition(r, 3));
	++r;

	_button_sizer = new wxBoxSizer(wxHORIZONTAL);
	_remake_certificates = new Button(this, _("Re-make certificates and key..."));
	_button_sizer->Add(_remake_certificates, 1, wxRIGHT, border);
	table->Add(_button_sizer, wxGBPosition(r, 0), wxGBSpan(1, 4));
	++r;

	_private_key_bad = new StaticText(this, _("Leaf private key does not match leaf certificate!"));
	font = *wxSMALL_FONT;
	font.SetWeight(wxFONTWEIGHT_BOLD);
	_private_key_bad->SetFont(font);
	table->Add(_private_key_bad, wxGBPosition(r, 0), wxGBSpan(1, 3));
	++r;

	_add_certificate->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::add_certificate, this));
	_remove_certificate->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::remove_certificate, this));
	_export_certificate->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::export_certificate, this));
	_certificates->Bind(wxEVT_LIST_ITEM_SELECTED, bind(&CertificateChainEditor::update_sensitivity, this));
	_certificates->Bind(wxEVT_LIST_ITEM_DESELECTED, bind(&CertificateChainEditor::update_sensitivity, this));
	_remake_certificates->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::remake_certificates, this));
	_export_chain->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::export_chain, this));
	_import_private_key->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::import_private_key, this));
	_export_private_key->Bind(wxEVT_BUTTON, bind(&CertificateChainEditor::export_private_key, this));

	auto buttons = CreateSeparatedButtonSizer(wxCLOSE);
	if (buttons) {
		_sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit(_sizer);

	update_certificate_list();
	update_private_key();
	update_sensitivity();
}

void
CertificateChainEditor::add_button(wxWindow* button)
{
	_button_sizer->Add(button, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_sizer->Layout();
}

void
CertificateChainEditor::add_certificate()
{
	wxFileDialog dialog(this, _("Select Certificate file"), {}, {}, char_to_wx("PEM files (*.pem)|*.pem|KEY files (*.key)|*.key|All files (*.*)|*.*"));

	if (dialog.ShowModal() == wxID_OK) {
		try {
			dcp::Certificate c;
			string extra;
			try {
				extra = c.read_string(dcp::file_to_string(wx_to_std(dialog.GetPath())));
			} catch (boost::filesystem::filesystem_error& e) {
				error_dialog(this, _("Could not import certificate (%s)"), dialog.GetPath());
				return;
			}

			if (!extra.empty()) {
				message_dialog(
					this,
					_("This file contains other certificates (or other data) after its first certificate. "
					  "Only the first certificate will be used.")
					);
			}
			auto chain = make_shared<dcp::CertificateChain>(*_get().get());
			chain->add(c);
			if (!chain->chain_valid()) {
				error_dialog(
					this,
					_("Adding this certificate would make the chain inconsistent, so it will not be added. "
					  "Add certificates in order from root to intermediate to leaf.")
					);
				chain->remove(c);
			} else {
				_set(chain);
				update_certificate_list();
			}
		} catch (dcp::MiscError& e) {
			error_dialog(this, _("Could not read certificate file."), std_to_wx(e.what()));
		}
	}

	update_sensitivity();
}

void
CertificateChainEditor::remove_certificate()
{
	if (_nag_alter()) {
		/* Cancel was clicked */
		return;
	}

	int i = _certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	_certificates->DeleteItem(i);
	auto chain = make_shared<dcp::CertificateChain>(*_get().get());
	chain->remove(i);
	_set(chain);

	update_sensitivity();
	update_certificate_list();
}

void
CertificateChainEditor::export_certificate()
{
	int i = _certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	auto all = _get()->root_to_leaf();

	wxString default_name;
	if (i == 0) {
		default_name = char_to_wx("root.pem");
	} else if (i == static_cast<int>(all.size() - 1)) {
		default_name = char_to_wx("leaf.pem");
	} else {
		default_name = char_to_wx("intermediate.pem");
	}

	wxFileDialog dialog(
		this, _("Select Certificate File"), wxEmptyString, default_name, char_to_wx("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	auto j = all.begin();
	for (int k = 0; k < i; ++k) {
		++j;
	}

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

	string const s = j->certificate(true);
	f.checked_write(s.c_str(), s.length());
}

void
CertificateChainEditor::export_chain()
{
	wxFileDialog dialog(
		this, _("Select Chain File"), wxEmptyString, char_to_wx("certificate_chain.pem"), char_to_wx("PEM files (*.pem)|*.pem"),
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

	auto const s = _get()->chain();
	f.checked_write(s.c_str(), s.length());
}

void
CertificateChainEditor::update_certificate_list()
{
	_certificates->DeleteAllItems();
	size_t n = 0;
	auto certs = _get()->root_to_leaf();
	for (auto const& i: certs) {
		wxListItem item;
		item.SetId(n);
		_certificates->InsertItem(item);
		_certificates->SetItem(n, 1, std_to_wx(i.thumbprint()));

		if (n == 0) {
			_certificates->SetItem(n, 0, _("Root"));
		} else if (n == (certs.size() - 1)) {
			_certificates->SetItem(n, 0, _("Leaf"));
		} else {
			_certificates->SetItem(n, 0, _("Intermediate"));
		}

		++n;
	}

	static wxColour normal = _private_key_bad->GetForegroundColour();

	if (_get()->private_key_valid()) {
		_private_key_bad->Hide();
		_private_key_bad->SetForegroundColour(normal);
	} else {
		_private_key_bad->Show();
		_private_key_bad->SetForegroundColour(wxColour(255, 0, 0));
	}
}

void
CertificateChainEditor::remake_certificates()
{
	if (_nag_alter()) {
		/* Cancel was clicked */
		return;
	}

	MakeChainDialog dialog(this, _get());

	if (dialog.ShowModal() == wxID_OK) {
		_set(dialog.get());
		update_certificate_list();
		update_private_key();
	}
}

void
CertificateChainEditor::update_sensitivity()
{
	/* We can only remove the leaf certificate */
	_remove_certificate->Enable(_certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) == (_certificates->GetItemCount() - 1));
	_export_certificate->Enable(_certificates->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
}

void
CertificateChainEditor::update_private_key()
{
	checked_set(_private_key, dcp::private_key_fingerprint(_get()->key().get()));
	_sizer->Layout();
}

void
CertificateChainEditor::import_private_key()
{
	wxFileDialog dialog(this, _("Select Key file"), {}, {}, char_to_wx("PEM files (*.pem)|*.pem|KEY files (*.key)|*.key|All files (*.*)|*.*"));

	if (dialog.ShowModal() == wxID_OK) {
		try {
			boost::filesystem::path p(wx_to_std(dialog.GetPath()));
			if (dcp::filesystem::file_size(p) > 8192) {
				error_dialog(
					this,
					wxString::Format(_("Could not read key file; file is too long (%s)"), std_to_wx(p.string()))
					);
				return;
			}

			auto chain = make_shared<dcp::CertificateChain>(*_get().get());
			chain->set_key(dcp::file_to_string(p));
			_set(chain);
			update_private_key();
		} catch (std::exception& e) {
			error_dialog(this, _("Could not read certificate file."), std_to_wx(e.what()));
		}
	}

	update_sensitivity();
	update_certificate_list();
}

void
CertificateChainEditor::export_private_key()
{
	auto key = _get()->key();
	if (!key) {
		return;
	}

	wxFileDialog dialog(
		this, _("Select Key File"), wxEmptyString, char_to_wx("private_key.pem"), char_to_wx("PEM files (*.pem)|*.pem"),
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
		);

	if (dialog.ShowModal() == wxID_OK) {
		boost::filesystem::path path(wx_to_std(dialog.GetPath()));
		if (path.extension() != ".pem") {
			path += ".pem";
		}
		dcp::File f(path, "w");
		if (!f) {
			throw OpenFileError(path, f.open_error(), OpenFileError::WRITE);
		}

		auto const s = _get()->key().get();
		f.checked_write(s.c_str(), s.length());
	}
}

