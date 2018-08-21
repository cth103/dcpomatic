/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "message_dialog.h"
#include "wx_util.h"

MessageDialog::MessageDialog (wxWindow* parent, wxString title, wxString message)
	: wxDialog (parent, wxID_ANY, title)
{
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxStaticText* text = new wxStaticText (this, wxID_ANY, message);
	sizer->Add (text, 1, wxALL, DCPOMATIC_DIALOG_BORDER);
	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
        }
	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}
