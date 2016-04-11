/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "editable_list.h"
#include "email_dialog.h"
#include <wx/wx.h>
#include <list>
#include <vector>

class CinemaDialog : public wxDialog
{
public:
	CinemaDialog (
		wxWindow *,
		std::string,
		std::string name = "",
		std::list<std::string> emails = std::list<std::string> (),
		int utc_offset_hour = 0,
		int utc_offset_minute = 0
		);

	std::string name () const;
	std::list<std::string> emails () const;
	int utc_offset_hour () const;
	int utc_offset_minute () const;

private:
	std::vector<std::string> get_emails () const;
	void set_emails (std::vector<std::string>);

	wxTextCtrl* _name;
	EditableList<std::string, EmailDialog>* _email_list;
	std::vector<std::string> _emails;
	wxChoice* _utc_offset;

	struct Offset
	{
		Offset (wxString n, int h, int m)
			: name (n)
			, hour (h)
			, minute (m)
		{}

		wxString name;
		int hour;
		int minute;
	};

	std::vector<Offset> _offsets;
};
