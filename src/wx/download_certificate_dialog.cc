/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "doremi_certificate_panel.h"
#include "dolby_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"

using boost::optional;

DownloadCertificateDialog::DownloadCertificateDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Download certificate"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	_notebook = new wxNotebook (this, wxID_ANY);
	sizer->Add (_notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	_pages.push_back (new DoremiCertificatePanel (_notebook, this));
	_setup.push_back (false);
	_notebook->AddPage (_pages.back(), _("Doremi"), true);
	_pages.push_back (new DolbyCertificatePanel (_notebook, this));
	_setup.push_back (false);
	_notebook->AddPage (_pages.back(), _("Dolby"), false);

	_download = new wxButton (this, wxID_ANY, _("Download"));
	sizer->Add (_download, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	_message = new wxStaticText (this, wxID_ANY, wxT (""));
	sizer->Add (_message, 0, wxALL, DCPOMATIC_SIZER_GAP);
	wxFont font = _message->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	_message->SetFont (font);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (sizer);

	_notebook->Bind (wxEVT_NOTEBOOK_PAGE_CHANGED, &DownloadCertificateDialog::page_changed, this);
	_download->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&DownloadCertificateDialog::download, this));
	_download->Enable (false);

	wxNotebookEvent ev;
	page_changed (ev);
}

DownloadCertificateDialog::~DownloadCertificateDialog ()
{
	_notebook->Unbind (wxEVT_NOTEBOOK_PAGE_CHANGED, &DownloadCertificateDialog::page_changed, this);
}

void
DownloadCertificateDialog::download ()
{
	_pages[_notebook->GetSelection()]->download (_message);
}

dcp::Certificate
DownloadCertificateDialog::certificate () const
{
	optional<dcp::Certificate> c = _pages[_notebook->GetSelection()]->certificate ();
	DCPOMATIC_ASSERT (c);
	return c.get ();
}

void
DownloadCertificateDialog::setup_sensitivity ()
{
	DownloadCertificatePanel* p = _pages[_notebook->GetSelection()];
	_download->Enable (p->ready_to_download ());
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(p->certificate ()));
	}

}

void
DownloadCertificateDialog::page_changed (wxNotebookEvent &)
{
	int const n = _notebook->GetSelection();
	if (!_setup[n]) {
		_pages[n]->setup ();
		_setup[n] = true;
	}

	setup_sensitivity ();
}
