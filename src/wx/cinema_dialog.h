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
#include <wx/wx.h>
#include <list>
#include <vector>


class CinemaDialog : public wxDialog
{
public:
	CinemaDialog (
		wxWindow *,
		wxString,
		std::string name = "",
		std::list<std::string> emails = std::list<std::string> (),
		std::string notes = "",
		int utc_offset_hour = 0,
		int utc_offset_minute = 0
		);

	std::string name () const;
	std::string notes () const;
	std::list<std::string> emails () const;
	int utc_offset_hour () const;
	int utc_offset_minute () const;

private:
	std::vector<std::string> get_emails () const;
	void set_emails (std::vector<std::string>);

	wxTextCtrl* _name;
	wxTextCtrl* _notes;
	EditableList<std::string, EmailDialog>* _email_list;
	std::vector<std::string> _emails;
	wxChoice* _utc_offset;
	std::vector<Offset> _offsets;
};
