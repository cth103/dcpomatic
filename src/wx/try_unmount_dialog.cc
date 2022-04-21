/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "static_text.h"
#include "try_unmount_dialog.h"
#include "wx_util.h"
#include <wx/wx.h>


static int constexpr width = 300;


TryUnmountDialog::TryUnmountDialog (wxWindow* parent, wxString description)
	: wxDialog (parent, wxID_ANY, _("DCP-o-matic Disk Writer"))
{
	auto sizer = new wxBoxSizer (wxVERTICAL);
	auto text = new StaticText (this, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
	sizer->Add (text, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	text->SetLabelMarkup (
		wxString::Format(_("The drive <b>%s</b> is mounted.\n\nIt must be unmounted before DCP-o-matic can write to it.\n\nShould DCP-o-matic try to unmount it now?"), description)
		);
	text->Wrap(width);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}

