/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_appearance_dialog.h"
#include "lib/subrip_content.h"
#include <wx/wx.h>
#include <wx/clrpicker.h>

using boost::shared_ptr;

SubtitleAppearanceDialog::SubtitleAppearanceDialog (wxWindow* parent, shared_ptr<SubRipContent> content)
	: TableDialog (parent, _("Subtitle appearance"), 2, 1, true)
	, _content (content)
{
	add (_("Colour"), true);
	_colour = new wxColourPickerCtrl (this, wxID_ANY);
	add (_colour);

	_outline = new wxCheckBox (this, wxID_ANY, _("Outline"));
	add (_outline);
	add_spacer ();

	add (_("Outline colour"), true);
	_outline_colour = new wxColourPickerCtrl (this, wxID_ANY);
	add (_outline_colour);

	layout ();

	_colour->SetColour (wxColour (_content->colour().r, _content->colour().g, _content->colour().b));
	_outline->SetValue (_content->outline ());
	_outline_colour->SetColour (wxColour (_content->outline_colour().r, _content->outline_colour().g, _content->outline_colour().b));
}

void
SubtitleAppearanceDialog::apply ()
{
	wxColour const c = _colour->GetColour ();
	_content->set_colour (dcp::Colour (c.Red(), c.Green(), c.Blue()));
	_content->set_outline (_outline->GetValue ());
	wxColour const oc = _outline_colour->GetColour ();
	_content->set_outline_colour (dcp::Colour (oc.Red(), oc.Green(), oc.Blue()));
}
