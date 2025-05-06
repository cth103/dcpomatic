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
	Append(wxString{});
	set(0);
}


void
Choice::add_entry(string const& entry)
{
	add_entry(std_to_wx(entry));
}


void
Choice::add_entry(wxString const& entry)
{
	if (_needs_clearing) {
		Clear();
		_needs_clearing = false;
	}

	Append(entry);
}


void
Choice::add_entry(wxString const& entry, wxClientData* data)
{
	if (_needs_clearing) {
		Clear();
		_needs_clearing = false;
	}

	Append(entry, data);
}


void
Choice::add_entry(wxString const& entry, wxString const& data)
{
	if (_needs_clearing) {
		Clear();
		_needs_clearing = false;
	}

	Append(entry, new wxStringClientData(data));
}


void
Choice::add_entry(wxString const& entry, string const& data)
{
	add_entry(entry, std_to_wx(data));
}


void
Choice::set_entries(wxArrayString const& entries)
{
	if (GetStrings() == entries) {
		return;
	}

	Clear();
	Set(entries);
}


void
Choice::set(int index)
{
	SetSelection(index);
}


void
Choice::set_by_data(wxString const& data)
{
	for (unsigned int i = 0; i < GetCount(); ++i) {
		if (auto client_data = dynamic_cast<wxStringClientData*>(GetClientObject(i))) {
			if (client_data->GetData() == data) {
				set(i);
				return;
			}
		}
	}
}


void
Choice::set_by_data(string const& data)
{
	set_by_data(std_to_wx(data));
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


optional<string>
Choice::get_data() const
{

	auto index = get();
	if (!index) {
		return {};
	}

	return wx_to_std(dynamic_cast<wxStringClientData*>(GetClientObject(*index))->GetData());
}


void
Choice::clear()
{
	Clear();
}


int
Choice::size() const
{
	if (_needs_clearing) {
		return 0;
	}
	return GetCount();
}

