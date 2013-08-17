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

#include <wx/statline.h>
#include "lib/colour_conversion.h"
#include "wx_util.h"
#include "preset_colour_conversion_dialog.h"
#include "colour_conversion_editor.h"

using std::string;
using std::cout;

PresetColourConversionDialog::PresetColourConversionDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Colour conversion"))
	, _editor (new ColourConversionEditor (this))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	add_label_to_sizer (table, this, _("Name"), true);
	_name = new wxTextCtrl (this, wxID_ANY, wxT (""));
	table->Add (_name);

	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	overall_sizer->Add (new wxStaticLine (this, wxID_ANY), 0, wxEXPAND);
	overall_sizer->Add (_editor);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

PresetColourConversion
PresetColourConversionDialog::get () const
{
	PresetColourConversion pc;
	pc.name = wx_to_std (_name->GetValue ());
	pc.conversion = _editor->get ();
	return pc;
}

void
PresetColourConversionDialog::set (PresetColourConversion c)
{
	_name->SetValue (std_to_wx (c.name));
	_editor->set (c.conversion);
}
