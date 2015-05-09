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

#include "wx_util.h"
#include "download_certificate_dialog.h"
#include <boost/bind.hpp>

using boost::function;

DownloadCertificateDialog::DownloadCertificateDialog (wxWindow* parent, function<void (boost::filesystem::path)> load)
	: TableDialog (parent, _("Download certificate"), 2, true)
	, _load (load)
	, _message (0)
	, _download (0)
{

}

void
DownloadCertificateDialog::add_common_widgets ()
{
	add_spacer ();
	_download = add (new wxButton (this, wxID_ANY, _("Download")));

	add_spacer ();
	_message = add (new wxStaticText (this, wxID_ANY, wxT ("")));

	wxFont font = _message->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	_message->SetFont (font);
	
	_download->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&DownloadCertificateDialog::download, this));
	_download->Enable (false);

	layout ();

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (false);
}

void
DownloadCertificateDialog::downloaded (bool done)
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (done);
}

	
