/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/colour_conversion.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class ColourConversionEditor;


class ContentColourConversionDialog : public wxDialog
{
public:
	ContentColourConversionDialog (wxWindow *, bool yuv);

	void set (ColourConversion);
	ColourConversion get () const;

private:
	void check_for_preset ();
	void preset_check_clicked ();
	void preset_choice_changed ();

	CheckBox* _preset_check;
	wxChoice* _preset_choice;
	ColourConversionEditor* _editor;
	bool _setting;

	boost::signals2::scoped_connection _editor_connection;
};

