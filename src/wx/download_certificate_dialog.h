/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/warnings.h"
#include <dcp/certificate.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
#include <wx/notebook.h>
DCPOMATIC_ENABLE_WARNINGS

class DownloadCertificatePanel;

class DownloadCertificateDialog : public wxDialog
{
public:
	explicit DownloadCertificateDialog (wxWindow* parent);
	~DownloadCertificateDialog ();

	dcp::Certificate certificate () const;
	std::string url () const;

	void setup_sensitivity ();

	wxNotebook* notebook () const {
		return _notebook;
	}

	wxStaticText* message () const {
		return _message;
	}

private:
	void download ();
	void page_changed (wxNotebookEvent &);

	wxNotebook* _notebook;
	std::vector<DownloadCertificatePanel*> _pages;
	wxButton* _download;
	wxStaticText* _message;
};
