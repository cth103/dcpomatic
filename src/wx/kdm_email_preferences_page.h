/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include "preferences_page.h"


class EmailDialog;


namespace dcpomatic {
namespace preferences {


class KDMEmailPage : public preferences::Page
{
public:
	KDMEmailPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
#endif

private:
	void setup() override;
	void config_changed() override;
	void kdm_subject_changed();
	void kdm_from_changed();
	void kdm_bcc_changed();
	void kdm_email_changed();
	void reset_email();

	wxTextCtrl* _subject;
	wxTextCtrl* _from;
	EditableList<std::string, EmailDialog>* _cc;
	wxTextCtrl* _bcc;
	wxTextCtrl* _email;
	wxButton* _reset_email;
};


}
}

