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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>
#include <boost/optional.hpp>


class Choice : public wxChoice
{
public:
	Choice(wxWindow* parent);

	void add_entry(wxString const& entry);
	void add_entry(wxString const& entry, wxClientData* data);
	void add_entry(wxString const& entry, wxString const& data);
	void add_entry(wxString const& entry, std::string const& data);
	void add_entry(std::string const& entry);
	void set_entries(wxArrayString const& entries);

	void clear();
	int size() const;

	void set(int index);
	void set_by_data(wxString const& data);
	void set_by_data(std::string const& data);
	boost::optional<int> get() const;
	boost::optional<wxString> get_data() const;

	template <typename... Args>
	void bind(Args... args) {
		Bind(wxEVT_CHOICE, boost::bind(std::forward<Args>(args)...));
	}

private:
	bool _needs_clearing = true;
};

