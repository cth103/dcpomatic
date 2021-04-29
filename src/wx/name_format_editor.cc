/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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
#include "static_text.h"
#include "lib/util.h"


using std::string;


NameFormatEditor::NameFormatEditor (wxWindow* parent, dcp::NameFormat name, dcp::NameFormat::Map titles, dcp::NameFormat::Map examples, string suffix)
	: _panel (new wxPanel(parent))
	, _example (new StaticText(_panel, ""))
	, _sizer (new wxBoxSizer(wxVERTICAL))
	, _specification (new wxTextCtrl(_panel, wxID_ANY, ""))
	, _name (name)
	, _examples (examples)
	, _suffix (suffix)
{
	_sizer->Add (_specification, 0, wxEXPAND, DCPOMATIC_SIZER_Y_GAP);
	if (!_examples.empty ()) {
		_sizer->Add (_example, 0, wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
	}
	_panel->SetSizer (_sizer);

	for (auto const& i: titles) {
		auto t = new StaticText (_panel, std_to_wx (String::compose ("%%%1 %2", i.first, i.second)));
		_sizer->Add (t);
		auto font = t->GetFont();
		font.SetStyle (wxFONTSTYLE_ITALIC);
		font.SetPointSize (font.GetPointSize() - 1);
		t->SetFont (font);
		t->SetForegroundColour (wxColour (0, 0, 204));
	}

	_specification->SetValue (std_to_wx (_name.specification()));
	_specification->Bind (wxEVT_TEXT, boost::bind(&NameFormatEditor::changed, this));

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
	if (_examples.empty ()) {
		return;
	}

	_name.set_specification (careful_string_filter(wx_to_std(_specification->GetValue())));

	auto example = wxString::Format (_("e.g. %s"), std_to_wx (_name.get (_examples, _suffix)));
	wxString wrapped;
	for (size_t i = 0; i < example.Length(); ++i) {
		if (i > 0 && (i % 40) == 0) {
			wrapped += "\n";
		}
		wrapped += example[i];
	}

	_example->SetLabel (wrapped);
}
