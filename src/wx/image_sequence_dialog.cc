/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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


#include "image_sequence_dialog.h"
#include "wx_util.h"
#include <dcp/locale_convert.h>


using dcp::locale_convert;


ImageSequenceDialog::ImageSequenceDialog (wxWindow* parent)
	: TableDialog (parent, _("Add image sequence"), 2, 1, true)
{
	add (_("Frame rate"), true);
	_frame_rate = add(new wxTextCtrl(this, wxID_ANY, char_to_wx("24")));
	layout ();
}

double
ImageSequenceDialog::frame_rate () const
{
	try {
		return locale_convert<double> (wx_to_std (_frame_rate->GetValue ()));
	} catch (...) {

	}

	return 0;
}
