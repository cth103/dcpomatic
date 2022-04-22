/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "table_dialog.h"
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/spinctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


class CustomScaleDialog : public TableDialog
{
public:
	CustomScaleDialog (wxWindow* parent, dcp::Size initial, dcp::Size film_container, boost::optional<float> custom_ratio, boost::optional<dcp::Size> custom_size);

	boost::optional<float> custom_ratio () const;
	boost::optional<dcp::Size> custom_size () const;

private:
	void update_size_from_ratio ();
	void update_ratio_from_size ();
	void setup_sensitivity ();

	wxRadioButton* _ratio_to_fit;
	wxTextCtrl* _ratio;
	wxStaticText* _size_from_ratio;
	wxRadioButton* _size;
	wxSpinCtrl* _width;
	wxSpinCtrl* _height;
	wxStaticText* _ratio_from_size;

	dcp::Size _film_container;
};

