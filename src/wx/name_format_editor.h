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
#include <boost/signals2.hpp>

class NameFormatEditor
{
public:
	NameFormatEditor (wxWindow* parent, dcp::NameFormat name, dcp::NameFormat::Map titles, dcp::NameFormat::Map examples, std::string suffix);

	wxPanel* panel () const {
		return _panel;
	}

	dcp::NameFormat get () const {
		return _name;
	}

	boost::signals2::signal<void ()> Changed;

private:

	void changed ();
	void update_example ();

	wxPanel* _panel;
	wxStaticText* _example;
	wxSizer* _sizer;
	wxTextCtrl* _specification;

	dcp::NameFormat _name;
	dcp::NameFormat::Map _examples;
	std::string _suffix;
};

#endif
