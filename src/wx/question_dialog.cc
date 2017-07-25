/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "question_dialog.h"

QuestionDialog::QuestionDialog (wxWindow* parent, wxString title, wxString affirmative, wxString negative)
	: wxDialog (parent, wxID_ANY, title)
	, _affirmative (affirmative)
	, _negative (negative)
{
	_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_sizer);
}

void
QuestionDialog::layout ()
{
	wxStdDialogButtonSizer* buttons = CreateStdDialogButtonSizer (0);
	_sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	buttons->SetAffirmativeButton (new wxButton (this, wxID_OK, _affirmative));
	buttons->SetNegativeButton (new wxButton (this, wxID_CANCEL, _negative));
	buttons->Realize ();

	_sizer->Layout ();
	_sizer->SetSizeHints (this);
}
