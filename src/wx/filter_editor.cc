/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/filter_editor.cc
 *  @brief A panel to select FFmpeg filters.
 */

#include <iostream>
#include <algorithm>
#include "lib/filter.h"
#include "filter_editor.h"
#include "wx_util.h"

using namespace std;

FilterEditor::FilterEditor (wxWindow* parent, vector<Filter const *> const & active)
	: wxPanel (parent)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (sizer);

	vector<Filter const *> filters = Filter::all ();

	typedef map<string, list<Filter const *> > CategoryMap;
	CategoryMap categories;

	for (vector<Filter const *>::iterator i = filters.begin(); i != filters.end(); ++i) {
		CategoryMap::iterator j = categories.find ((*i)->category ());
		if (j == categories.end ()) {
			list<Filter const *> c;
			c.push_back (*i);
			categories[(*i)->category()] = c;
		} else {
			j->second.push_back (*i);
		}
	}

	for (CategoryMap::iterator i = categories.begin(); i != categories.end(); ++i) {

		wxStaticText* c = new wxStaticText (this, wxID_ANY, std_to_wx (i->first));
		wxFont font = c->GetFont();
		font.SetWeight(wxFONTWEIGHT_BOLD);
		c->SetFont(font);
		sizer->Add (c);

		for (list<Filter const *>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
			wxCheckBox* b = new wxCheckBox (this, wxID_ANY, std_to_wx ((*j)->name ()));
			bool const a = find (active.begin(), active.end(), *j) != active.end ();
			b->SetValue (a);
			_filters[*j] = b;
			b->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&FilterEditor::filter_toggled, this));
			sizer->Add (b);
		}

		sizer->AddSpacer (6);
	}
}

void
FilterEditor::filter_toggled ()
{
	ActiveChanged ();
}

vector<Filter const*>
FilterEditor::active () const
{
	vector<Filter const *> active;
	for (map<Filter const *, wxCheckBox*>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		if (i->second->IsChecked ()) {
			active.push_back (i->first);
		}
	}

	return active;
}
