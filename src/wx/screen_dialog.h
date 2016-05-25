/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "editable_list.h"
#include <dcp/certificate.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>

class Progress;
class CertificateFileDialogWrapper;

class ScreenDialog : public wxDialog
{
public:
	ScreenDialog (
		wxWindow *,
		wxString,
		std::string name = "",
		std::string notes = "",
		boost::optional<dcp::Certificate> c = boost::optional<dcp::Certificate> (),
		std::vector<dcp::Certificate> d = std::vector<dcp::Certificate> ()
		);

	std::string name () const;
	std::string notes () const;
	boost::optional<dcp::Certificate> recipient () const;
	std::vector<dcp::Certificate> trusted_devices () {
		return _trusted_devices;
	}

private:
	void get_recipient_from_file ();
	void load_recipient (boost::filesystem::path);
	void download_recipient ();
	void setup_sensitivity ();
	void set_recipient (boost::optional<dcp::Certificate>);

	void set_trusted_devices (std::vector<dcp::Certificate> d) {
		_trusted_devices = d;
	}

	wxGridBagSizer* _sizer;
	wxTextCtrl* _name;
	wxTextCtrl* _notes;
	wxStaticText* _recipient_thumbprint;
	wxButton* _get_recipient_from_file;
	wxButton* _download_recipient;
	EditableList<dcp::Certificate, CertificateFileDialogWrapper>* _trusted_device_list;

	boost::optional<dcp::Certificate> _recipient;
	std::vector<dcp::Certificate> _trusted_devices;
};
