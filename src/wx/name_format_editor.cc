/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "name_format_editor.h"
#include "wx_util.h"

NameFormatEditor::NameFormatEditor (wxWindow* parent, dcp::NameFormat name, dcp::NameFormat::Map titles, dcp::NameFormat::Map examples)
	: _panel (new wxPanel (parent))
	, _example (new wxStaticText (_panel, wxID_ANY, ""))
	, _sizer (new wxBoxSizer (wxVERTICAL))
	, _specification (new wxTextCtrl (_panel, wxID_ANY, ""))
	, _name (name)
	, _examples (examples)
{
	_sizer->Add (_specification, 0, wxEXPAND, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (_example, 0, wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
	_panel->SetSizer (_sizer);

	for (dcp::NameFormat::Map::const_iterator i = titles.begin(); i != titles.end(); ++i) {
		wxStaticText* t = new wxStaticText (_panel, wxID_ANY, std_to_wx (String::compose ("%%%1 %2", i->first, i->second)));
		_sizer->Add (t);
		wxFont font = t->GetFont();
		font.SetStyle (wxFONTSTYLE_ITALIC);
		font.SetPointSize (font.GetPointSize() - 1);
		t->SetFont (font);
		t->SetForegroundColour (wxColour (0, 0, 204));
	}

	_specification->SetValue (std_to_wx (_name.specification ()));
	_specification->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&NameFormatEditor::changed, this));

	update_example ();
}

void
NameFormatEditor::changed ()
{
	update_example ();
	Changed ();
}

void
NameFormatEditor::update_example ()
{
	_name.set_specification (wx_to_std (_specification->GetValue ()));

	wxString example = wxString::Format (_("e.g. %s"), _name.get (_examples));
	wxString wrapped;
	for (size_t i = 0; i < example.Length(); ++i) {
		if (i > 0 && (i % 40) == 0) {
			wrapped += "\n";
		}
		wrapped += example[i];
	}

	_example->SetLabel (wrapped);
}
