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

/** @file  src/filter_view.cc
 *  @brief A panel to select FFmpeg filters.
 */

#include <iostream>
#include <algorithm>
#include "lib/filter.h"
#include "filter_view.h"
#include "wx_util.h"

using namespace std;

FilterView::FilterView (wxWindow* parent, vector<Filter const *> const & active)
	: wxPanel (parent)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (sizer);
	
	vector<Filter const *> filters = Filter::all ();

	for (vector<Filter const *>::iterator i = filters.begin(); i != filters.end(); ++i) {
		wxCheckBox* b = new wxCheckBox (this, wxID_ANY, std_to_wx ((*i)->name ()));
		bool const a = find (active.begin(), active.end(), *i) != active.end ();
		b->SetValue (a);
		_filters[*i] = b;
		b->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilterView::filter_toggled), 0, this);
		sizer->Add (b);
	}
}

void
FilterView::filter_toggled (wxCommandEvent &)
{
	ActiveChanged ();
}

vector<Filter const*>
FilterView::active () const
{
	vector<Filter const *> active;
	for (map<Filter const *, wxCheckBox*>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		if (i->second->IsChecked ()) {
			active.push_back (i->first);
		}
	}

	return active;
}
