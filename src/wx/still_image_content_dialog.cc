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

#include <wx/spinctrl.h>
#include "lib/still_image_content.h"
#include "still_image_content_dialog.h"
#include "wx_util.h"

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

StillImageContentDialog::StillImageContentDialog (wxWindow* parent, shared_ptr<StillImageContent> content)
	: wxDialog (parent, wxID_ANY, _("Stlll Image"))
	, _content (content)
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (3, 6, 6);
	grid->AddGrowableCol (1, 1);

	{
		add_label_to_sizer (grid, this, _("Duration"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_video_length = new wxSpinCtrl (this);
		s->Add (_video_length);
		/// TRANSLATORS: this is an abbreviation for seconds, the unit of time
		add_label_to_sizer (s, this, _("s"), false);
		grid->Add (s);
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (grid, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	checked_set (_video_length, content->video_length () / 24);
	_video_length->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (StillImageContentDialog::video_length_changed), 0, this);
}

void
StillImageContentDialog::video_length_changed (wxCommandEvent &)
{
	shared_ptr<StillImageContent> c = _content.lock ();
	if (!c) {
		return;
	}
	
	c->set_video_length (_video_length->GetValue() * 24);
}
