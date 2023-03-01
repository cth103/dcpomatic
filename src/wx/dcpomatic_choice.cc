/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_choice.h"
#include "wx_util.h"


using std::string;
using boost::optional;


Choice::Choice(wxWindow* parent)
	: wxChoice(parent, wxID_ANY)
{
	/* This hack works around a problem where the height of the wxChoice would be
	 * too small on KDE.  This added empty string will be removed in the first
	 * call to add().
	 */
	Append("");
	set(0);
}


void
Choice::add(string const& entry)
{
	add(std_to_wx(entry));
}


void
Choice::add(wxString const& entry)
{
	if (_needs_clearing) {
		Clear();
		_needs_clearing = false;
	}

	Append(entry);
}


void
Choice::add(wxString const& entry, wxClientData* data)
{
	if (_needs_clearing) {
		Clear();
		_needs_clearing = false;
	}

	Append(entry, data);
}


void
Choice::set(int index)
{
	SetSelection(index);
}


optional<int>
Choice::get() const
{
	auto const sel = GetSelection();
	if (sel == wxNOT_FOUND) {
		return {};
	}

	return sel;
}


optional<wxString>
Choice::get_data() const
{
	auto index = get();
	if (!index) {
		return {};
	}

	return dynamic_cast<wxStringClientData*>(GetClientObject(*index))->GetData();
}

