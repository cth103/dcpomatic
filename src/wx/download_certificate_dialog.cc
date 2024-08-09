/*
    Copyright (C) 2014-2022 Carl Hetherington <cth@carlh.net>

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


#include "barco_alchemy_certificate_panel.h"
#include "christie_certificate_panel.h"
#include "dcpomatic_button.h"
#include "dolby_doremi_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "gdc_certificate_panel.h"
#include "qube_certificate_panel.h"
#include "static_text.h"
#include "wx_util.h"


using std::string;
using boost::optional;


DownloadCertificateDialog::DownloadCertificateDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Download certificate"))
{
	auto sizer = new wxBoxSizer (wxVERTICAL);

	_notebook = new wxNotebook (this, wxID_ANY);
#ifdef DCPOMATIC_OSX
	_notebook->SetMinSize(wxSize(640, -1));
#endif
	sizer->Add (_notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	_download = new Button (this, _("Download"));
	sizer->Add (_download, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	_message = new StaticText(this, {});
	sizer->Add (_message, 0, wxALL, DCPOMATIC_SIZER_GAP);
	auto font = _message->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	_message->SetFont (font);

	_pages.push_back (new DolbyDoremiCertificatePanel (this));
	_pages.push_back (new BarcoAlchemyCertificatePanel (this));
	_pages.push_back (new ChristieCertificatePanel (this));
	_pages.push_back (new GDCCertificatePanel (this));
	_pages.push_back (new QubeCertificatePanel (this, N_("QXI")));
	_pages.push_back (new QubeCertificatePanel (this, N_("QXPD")));

	for (auto i: _pages) {
		_notebook->AddPage (i, i->name(), true);
	}

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (sizer);

	_notebook->Bind (wxEVT_NOTEBOOK_PAGE_CHANGED, &DownloadCertificateDialog::page_changed, this);
	_download->Bind (wxEVT_BUTTON, boost::bind (&DownloadCertificateDialog::download, this));
	_download->Enable (false);

	_notebook->SetSelection (0);

	SetMinSize (wxSize(640, -1));

	setup_sensitivity ();
}


DownloadCertificateDialog::~DownloadCertificateDialog ()
{
	_notebook->Unbind (wxEVT_NOTEBOOK_PAGE_CHANGED, &DownloadCertificateDialog::page_changed, this);
}


void
DownloadCertificateDialog::download ()
{
	_pages[_notebook->GetSelection()]->download ();
}


dcp::Certificate
DownloadCertificateDialog::certificate () const
{
	auto c = _pages[_notebook->GetSelection()]->certificate ();
	DCPOMATIC_ASSERT (c);
	return *c;
}


string
DownloadCertificateDialog::url () const
{
	auto u = _pages[_notebook->GetSelection()]->url();
	DCPOMATIC_ASSERT (u);
	return *u;
}


void
DownloadCertificateDialog::setup_sensitivity ()
{
	auto p = _pages[_notebook->GetSelection()];
	_download->Enable (p->ready_to_download ());
	auto ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(p->certificate ()));
	}
}


void
DownloadCertificateDialog::page_changed (wxNotebookEvent& ev)
{
	setup_sensitivity ();
	ev.Skip ();
}
