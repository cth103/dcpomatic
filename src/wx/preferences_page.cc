/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "preferences_page.h"
#include "lib/config.h"
#include <wx/wx.h>


using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;


Page::Page(wxSize panel_size, int border)
	: _border(border)
	, _panel(nullptr)
	, _panel_size(panel_size)
	, _window_exists(false)
{
	_config_connection = Config::instance()->Changed.connect(bind(&Page::config_changed_wrapper, this));
}


wxWindow*
Page::CreateWindow(wxWindow* parent)
{
	return create_window(parent);
}


wxWindow*
Page::create_window(wxWindow* parent)
{
	_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, _panel_size);
	auto s = new wxBoxSizer(wxVERTICAL);
	_panel->SetSizer(s);

	setup();
	_window_exists = true;
	config_changed();

	_panel->Bind(wxEVT_DESTROY, bind(&Page::window_destroyed, this));

	return _panel;
}

void
Page::config_changed_wrapper()
{
	if (_window_exists) {
		config_changed();
	}
}

void
Page::window_destroyed()
{
	_window_exists = false;
}


