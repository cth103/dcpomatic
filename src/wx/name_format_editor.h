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

#ifndef DCPOMATIC_NAME_FORMAT_EDITOR_H
#define DCPOMATIC_NAME_FORMAT_EDITOR_H

#include "lib/compose.hpp"
#include <dcp/name_format.h>
#include <wx/wx.h>
#include <boost/foreach.hpp>

template <class T>
class NameFormatEditor
{
public:
	NameFormatEditor (wxWindow* parent, T name, dcp::NameFormat::Map titles, dcp::NameFormat::Map examples)
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

		BOOST_FOREACH (char c, name.components ()) {
			wxStaticText* t = new wxStaticText (_panel, wxID_ANY, std_to_wx (String::compose ("%%%1 %2", c, titles[c])));
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

	wxPanel* panel () const {
		return _panel;
	}

	T get () const {
		return _name;
	}

	boost::signals2::signal<void ()> Changed;

private:

	void changed ()
	{
		update_example ();
		Changed ();
	}

	virtual void update_example ()
	{
		_name.set_specification (wx_to_std (_specification->GetValue ()));

		wxString example = wxString::Format (_("e.g. %s"), _name.get (_examples));
		wxString wrapped;
		for (size_t i = 0; i < example.Length(); ++i) {
			if (i > 0 && (i % 30) == 0) {
				wrapped += "\n";
			}
			wrapped += example[i];
		}

		_example->SetLabel (wrapped);
	}

	wxPanel* _panel;
	wxStaticText* _example;
	wxSizer* _sizer;
	wxTextCtrl* _specification;

	T _name;
	dcp::NameFormat::Map _examples;
};

#endif
