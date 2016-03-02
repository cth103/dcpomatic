/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "cinema_dialog.h"
#include "wx_util.h"
#include "lib/dcpomatic_assert.h"
#include <boost/foreach.hpp>

using std::string;
using std::vector;
using std::copy;
using std::back_inserter;
using std::list;
using std::cout;
using boost::bind;

static string
column (string s)
{
	return s;
}

CinemaDialog::CinemaDialog (wxWindow* parent, string title, string name, list<string> emails, int utc_offset)
	: wxDialog (parent, wxID_ANY, std_to_wx (title))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxGridBagSizer* sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	int r = 0;

	add_label_to_sizer (sizer, this, _("Name"), true, wxGBPosition (r, 0));
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (500, -1));
	sizer->Add (_name, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (sizer, this, _("UTC offset (time zone)"), true, wxGBPosition (r, 0));
	_utc_offset = new wxChoice (this, wxID_ANY);
	sizer->Add (_utc_offset, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (sizer, this, _("Email addresses for KDM delivery"), false, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	copy (emails.begin(), emails.end(), back_inserter (_emails));

	vector<string> columns;
	columns.push_back (wx_to_std (_("Address")));
	_email_list = new EditableList<string, EmailDialog> (
		this, columns, bind (&CinemaDialog::get_emails, this), bind (&CinemaDialog::set_emails, this, _1), bind (&column, _1)
		);

	sizer->Add (_email_list, wxGBPosition (r, 0), wxGBSpan (1, 2), wxEXPAND);
	++r;

	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	for (int i = -11; i <= -1; ++i) {
		_utc_offset->Append (wxString::Format (_("UTC%d"), i));
	}
	_utc_offset->Append (_("UTC"));
	for (int i = 1; i <= 12; ++i) {
		_utc_offset->Append (wxString::Format (_("UTC+%d"), i));
	}

	_utc_offset->SetSelection (utc_offset + 11);

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

string
CinemaDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

void
CinemaDialog::set_emails (vector<string> e)
{
	_emails = e;
}

vector<string>
CinemaDialog::get_emails () const
{
	return _emails;
}

list<string>
CinemaDialog::emails () const
{
	list<string> e;
	copy (_emails.begin(), _emails.end(), back_inserter (e));
	return e;
}

int
CinemaDialog::utc_offset () const
{
	return _utc_offset->GetSelection() - 11;
}
