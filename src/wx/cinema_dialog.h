/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
#include "table_dialog.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <list>
#include <vector>


class Choice;


class CinemaDialog : public wxDialog
{
public:
	CinemaDialog (
		wxWindow *,
		wxString,
		std::string name = "",
		std::vector<std::string> emails = std::vector<std::string>(),
		std::string notes = "",
		dcp::UTCOffset = {}
		);

	std::string name () const;
	std::string notes () const;
	std::vector<std::string> emails () const;
	dcp::UTCOffset utc_offset() const;

private:
	void set_emails (std::vector<std::string>);

	wxTextCtrl* _name;
	wxTextCtrl* _notes;
	EditableList<std::string, EmailDialog>* _email_list;
	std::vector<std::string> _emails;
	Choice* _utc_offset;
	std::vector<Offset> _offsets;
};
