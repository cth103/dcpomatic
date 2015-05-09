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

#ifndef DCPOMATIC_DOWNLOAD_CERTIFICATE_DIALOG_H
#define DCPOMATIC_DOWNLOAD_CERTIFICATE_DIALOG_H

#include <wx/wx.h>
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include "table_dialog.h"

class DownloadCertificateDialog : public TableDialog
{
public:
	DownloadCertificateDialog (wxWindow *, boost::function<void (boost::filesystem::path)>);

protected:
	void add_common_widgets ();
	void downloaded (bool done);
	
	boost::function<void (boost::filesystem::path)> _load;
	wxStaticText* _message;
	wxButton* _download;

private:
	virtual void download () = 0;
};

#endif
