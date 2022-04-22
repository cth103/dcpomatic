/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_DOWNLOAD_CERTIFICATE_PANEL_H
#define DCPOMATIC_DOWNLOAD_CERTIFICATE_PANEL_H


#include <dcp/warnings.h>
#include <dcp/certificate.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/optional.hpp>


class DownloadCertificateDialog;


class DownloadCertificatePanel : public wxPanel
{
public:
	DownloadCertificatePanel (DownloadCertificateDialog* dialog);

	virtual void do_download () = 0;
	virtual wxString name () const = 0;
	virtual bool ready_to_download () const;

	void download ();
	boost::optional<std::string> load_certificate (boost::filesystem::path, std::string url);
	boost::optional<std::string> load_certificate_from_chain (boost::filesystem::path, std::string url);
	boost::optional<dcp::Certificate> certificate () const;
	boost::optional<std::string> url () const;

protected:
	DownloadCertificateDialog* _dialog;
	wxFlexGridSizer* _table;
	wxTextCtrl* _serial;
	wxSizer* _overall_sizer;

private:
	boost::optional<dcp::Certificate> _certificate;
	boost::optional<std::string> _url;
};


#endif
