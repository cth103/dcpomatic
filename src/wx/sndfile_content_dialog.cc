/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "lib/util.h"
#include "lib/sndfile_content.h"
#include "sndfile_content_dialog.h"
#include "wx_util.h"

using boost::shared_ptr;

SndfileContentDialog::SndfileContentDialog (wxWindow* parent, shared_ptr<SndfileContent> content)
	: wxDialog (parent, wxID_ANY, _("Sound file"))
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (7, 6, 0);

	add_label_to_sizer (grid, this, wxT (""));
	add_label_to_sizer (grid, this, _("L"));
	add_label_to_sizer (grid, this, _("R"));
	add_label_to_sizer (grid, this, _("C"));
	add_label_to_sizer (grid, this, _("Lfe"));
	add_label_to_sizer (grid, this, _("Ls"));
	add_label_to_sizer (grid, this, _("Rs"));

	_buttons.resize (content->audio_channels ());
	for (int i = 0; i < content->audio_channels(); ++i) {

		if (content->audio_channels() == 1) {
			add_label_to_sizer (grid, this, _("Source"));
		} else {
			add_label_to_sizer (grid, this, wxString::Format (_("Source %d"), i + 1));
		}
		
		for (int j = 0; j < MAX_AUDIO_CHANNELS; ++j) {
			wxRadioButton* b = new wxRadioButton (this, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, j ? 0 : wxRB_GROUP);
			_buttons[i].push_back (b);
			grid->Add (b, wxSHRINK);
		}
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (grid, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}
