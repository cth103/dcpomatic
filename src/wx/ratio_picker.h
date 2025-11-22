/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include <wx/wx.h>
#include <boost/optional.hpp>
LIBDCP_DISABLE_WARNINGS
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS


class CheckBox;
class Choice;


class RatioPicker : public wxPanel
{
public:
	RatioPicker(wxWindow* parent, boost::optional<float> ratio);

	CheckBox* enable_checkbox() const {
		return _enable;
	}

	void set(boost::optional<float> ratio);

	boost::signals2::signal<void (boost::optional<float>)> Changed;

private:
	void enable_changed();
	void preset_changed();
	void custom_changed();

	void set_preset(boost::optional<float> ratio);
	void set_custom(boost::optional<float> ratio);
	void setup_sensitivity();

	CheckBox* _enable;
	Choice* _preset;
	wxTextCtrl* _custom;

	bool _ignore_changes = false;
};
