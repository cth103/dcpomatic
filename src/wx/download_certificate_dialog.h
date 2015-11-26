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

#include <wx/wx.h>
#include <wx/notebook.h>

class DownloadCertificatePanel;

class DownloadCertificateDialog : public wxDialog
{
public:
	DownloadCertificateDialog (wxWindow* parent);
	~DownloadCertificateDialog ();

	dcp::Certificate certificate () const;

	void setup_sensitivity ();

private:
	void download ();
	void page_changed (wxNotebookEvent &);

	wxNotebook* _notebook;
	std::vector<DownloadCertificatePanel*> _pages;
	std::vector<bool> _setup;
	wxButton* _download;
	wxStaticText* _message;
};
