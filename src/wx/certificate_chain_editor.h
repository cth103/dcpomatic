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


#include <wx/wx.h>
#include <memory>


namespace dcp {
	class CertificateChain;
}

class wxListCtrl;


class CertificateChainEditor : public wxDialog
{
public:
	CertificateChainEditor(
		wxWindow* parent,
		wxString title,
		int border,
		std::function<void (std::shared_ptr<dcp::CertificateChain>)> set,
		std::function<std::shared_ptr<const dcp::CertificateChain> (void)> get,
		std::function<bool (void)> nag_alter
		);

	void add_button(wxWindow* button);

private:
	void add_certificate();
	void remove_certificate();
	void export_certificate();
	void update_certificate_list();
	void remake_certificates();
	void update_sensitivity();
	void update_private_key();
	void import_private_key();
	void export_private_key();
	void export_chain();

	wxListCtrl* _certificates;
	wxButton* _add_certificate;
	wxButton* _export_certificate;
	wxButton* _remove_certificate;
	wxButton* _remake_certificates;
	wxStaticText* _private_key;
	wxButton* _import_private_key;
	wxButton* _export_private_key;
	wxButton* _export_chain;
	wxStaticText* _private_key_bad;
	wxSizer* _sizer;
	wxBoxSizer* _button_sizer;
	std::function<void (std::shared_ptr<dcp::CertificateChain>)> _set;
	std::function<std::shared_ptr<const dcp::CertificateChain> (void)> _get;
	std::function<bool (void)> _nag_alter;
};
