/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "password_entry.h"
#include "check_box.h"
#include "wx_util.h"

using std::string;
using boost::bind;

PasswordEntry::PasswordEntry (wxWindow* parent)
{
	_panel = new wxPanel (parent, wxID_ANY);
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_text = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
	sizer->Add (_text, 1, wxRIGHT, DCPOMATIC_SIZER_GAP);
	_show = new CheckBox (_panel, _("Show"));
	sizer->Add (_show, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	_panel->SetSizerAndFit (sizer);

	_show->Bind (wxEVT_CHECKBOX, bind(&PasswordEntry::show_clicked, this));
	_text->Bind (wxEVT_TEXT, bind(boost::ref(Changed)));
}

wxPanel *
PasswordEntry::get_panel () const
{
	return _panel;
}

void
PasswordEntry::show_clicked ()
{
	_panel->Freeze ();
	wxString const pass = _text->GetValue ();
	wxSizer* sizer = _text->GetContainingSizer ();
	long from, to;
	_text->GetSelection (&from, &to);
	sizer->Detach (_text);
	delete _text;
	_text = new wxTextCtrl (_panel, wxID_ANY, pass, wxDefaultPosition, wxDefaultSize, _show->GetValue() ? 0 : wxTE_PASSWORD);
	_text->SetSelection (from, to);
	_text->Bind (wxEVT_TEXT, bind(boost::ref(Changed)));
	sizer->Prepend (_text, 1, wxRIGHT, DCPOMATIC_SIZER_GAP);
	sizer->Layout ();
	_panel->Thaw ();
}

string
PasswordEntry::get () const
{
	return wx_to_std (_text->GetValue());
}

void
PasswordEntry::set (string s)
{
	_text->SetValue (std_to_wx(s));
}
