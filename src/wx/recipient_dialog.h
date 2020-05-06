/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "email_dialog.h"
#include "wx_util.h"
#include "lib/screen.h"
#include <dcp/certificate.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>

class Progress;
class TrustedDeviceDialog;

class RecipientDialog : public wxDialog
{
public:
	RecipientDialog (
		wxWindow *,
		wxString,
		std::string name = "",
		std::string notes = "",
		std::list<std::string> emails = std::list<std::string>(),
		int utc_offset_hour = 0,
		int utc_offset_minute = 0,
		boost::optional<dcp::Certificate> c = boost::optional<dcp::Certificate>()
		);

	std::string name () const;
	std::string notes () const;
	boost::optional<dcp::Certificate> recipient () const;
	std::list<std::string> emails () const;
	int utc_offset_hour () const;
	int utc_offset_minute () const;

private:
	void get_recipient_from_file ();
	void load_recipient (boost::filesystem::path);
	void setup_sensitivity ();
	void set_recipient (boost::optional<dcp::Certificate>);
	std::vector<std::string> get_emails () const;
	void set_emails (std::vector<std::string>);

	wxGridBagSizer* _sizer;
	wxTextCtrl* _name;
	wxTextCtrl* _notes;
	wxStaticText* _recipient_thumbprint;
	wxButton* _get_recipient_from_file;
	EditableList<std::string, EmailDialog>* _email_list;
	std::vector<std::string> _emails;
	wxChoice* _utc_offset;
	std::vector<Offset> _offsets;

	boost::optional<dcp::Certificate> _recipient;
};
