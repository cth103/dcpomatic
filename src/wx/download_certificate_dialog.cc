/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/bind.hpp>
#include "download_certificate_dialog.h"
#include "wx_util.h"

using boost::function;

DownloadCertificateDialog::DownloadCertificateDialog (wxWindow* parent, function<void (boost::filesystem::path)> load)
	: wxDialog (parent, wxID_ANY, _("Download certificate"))
	, _load (load)
{
	_overall_sizer = new wxBoxSizer (wxVERTICAL);
}

void
DownloadCertificateDialog::add_common_widgets ()
{
	_download = new wxButton (this, wxID_ANY, _("Download"));
	_overall_sizer->Add (_download, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);
	_gauge = new wxGauge (this, wxID_ANY, 100);
	_overall_sizer->Add (_gauge, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);
	_message = new wxStaticText (this, wxID_ANY, wxT (""));
	_overall_sizer->Add (_message, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		_overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (_overall_sizer);
	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);

	_download->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&DownloadCertificateDialog::download, this));
	_download->Enable (false);
}
