/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/util.h"
#include "wx_util.h"
#include "content_colour_conversion_dialog.h"
#include "colour_conversion_editor.h"
#include <wx/statline.h>
#include <boost/foreach.hpp>

using std::string;
using std::vector;
using std::cout;
using boost::optional;

ContentColourConversionDialog::ContentColourConversionDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Colour conversion"))
	, _editor (new ColourConversionEditor (this))
	, _setting (false)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_preset_check = new wxCheckBox (this, wxID_ANY, _("Use preset"));
	table->Add (_preset_check, 0, wxALIGN_CENTER_VERTICAL);
	_preset_choice = new wxChoice (this, wxID_ANY);
	table->Add (_preset_choice);

	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	overall_sizer->Add (new wxStaticLine (this, wxID_ANY), 0, wxEXPAND);
	overall_sizer->Add (_editor);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_preset_check->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&ContentColourConversionDialog::preset_check_clicked, this));
	_preset_choice->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ContentColourConversionDialog::preset_choice_changed, this));

	_editor_connection = _editor->Changed.connect (boost::bind (&ContentColourConversionDialog::check_for_preset, this));

	BOOST_FOREACH (PresetColourConversion const &i, PresetColourConversion::all ()) {
		_preset_choice->Append (std_to_wx (i.name));
	}
}

ColourConversion
ContentColourConversionDialog::get () const
{
	return _editor->get ();
}

void
ContentColourConversionDialog::set (ColourConversion c)
{
	_setting = true;
	_editor->set (c);
	_setting = false;

	check_for_preset ();
}

void
ContentColourConversionDialog::check_for_preset ()
{
	if (_setting) {
		return;
	}
	
	optional<size_t> preset = _editor->get().preset ();

	_preset_check->SetValue (preset);
	_preset_choice->Enable (preset);
	if (preset) {
		_preset_choice->SetSelection (preset.get ());
	} else {
		_preset_choice->SetSelection (-1);
	}
}

void
ContentColourConversionDialog::preset_check_clicked ()
{
	if (_preset_check->GetValue ()) {
		_preset_choice->SetSelection (0);
		preset_choice_changed ();
	} else {
		_preset_choice->SetSelection (-1);
		_preset_choice->Enable (false);
	}
}

void
ContentColourConversionDialog::preset_choice_changed ()
{
	vector<PresetColourConversion> presets = PresetColourConversion::all ();
	int const s = _preset_choice->GetCurrentSelection();
	if (s != -1) {
		set (presets[s].conversion);
	}
}

	
