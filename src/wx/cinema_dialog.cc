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


#include "cinema_dialog.h"
#include "wx_util.h"
#include "lib/dcpomatic_assert.h"
#include "lib/util.h"


using std::back_inserter;
using std::copy;
using std::cout;
using std::list;
using std::string;
using std::vector;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


CinemaDialog::CinemaDialog (wxWindow* parent, wxString title, string name, list<string> emails, string notes, int utc_offset_hour, int utc_offset_minute)
	: wxDialog (parent, wxID_ANY, title)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	auto sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	int r = 0;

	add_label_to_sizer (sizer, this, _("Name"), true, wxGBPosition(r, 0));
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx(name), wxDefaultPosition, wxSize(500, -1));
	sizer->Add (_name, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (sizer, this, _("UTC offset (time zone)"), true, wxGBPosition(r, 0));
	_utc_offset = new wxChoice (this, wxID_ANY);
	sizer->Add (_utc_offset, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (sizer, this, _("Notes"), true, wxGBPosition(r, 0));
	_notes = new wxTextCtrl (this, wxID_ANY, std_to_wx(notes), wxDefaultPosition, wxSize(500, -1));
	sizer->Add (_notes, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (sizer, this, _("Email addresses for KDM delivery"), false, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	copy (emails.begin(), emails.end(), back_inserter (_emails));

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Address"), 500, true));
	_email_list = new EditableList<string, EmailDialog> (
		this, columns, bind (&CinemaDialog::get_emails, this), bind (&CinemaDialog::set_emails, this, _1), [](string s, int) {
			return s;
		}, EditableListTitle::INVISIBLE, EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);

	sizer->Add (_email_list, wxGBPosition(r, 0), wxGBSpan(1, 2), wxEXPAND);
	++r;

	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	/* Default to UTC */
	size_t sel = get_offsets (_offsets);
	for (size_t i = 0; i < _offsets.size(); ++i) {
		_utc_offset->Append (_offsets[i].name);
		if (_offsets[i].hour == utc_offset_hour && _offsets[i].minute == utc_offset_minute) {
			sel = i;
		}
	}

	_utc_offset->SetSelection (sel);

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_name->SetFocus ();
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
	copy (_emails.begin(), _emails.end(), back_inserter(e));
	return e;
}


int
CinemaDialog::utc_offset_hour () const
{
	int const sel = _utc_offset->GetSelection();
	if (sel < 0 || sel > int(_offsets.size())) {
		return 0;
	}

	return _offsets[sel].hour;
}


int
CinemaDialog::utc_offset_minute () const
{
	int const sel = _utc_offset->GetSelection();
	if (sel < 0 || sel > int(_offsets.size())) {
		return 0;
	}

	return _offsets[sel].minute;
}


string
CinemaDialog::notes () const
{
	return wx_to_std (_notes->GetValue());
}
