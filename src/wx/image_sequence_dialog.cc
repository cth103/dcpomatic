/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <libdcp/raw_convert.h>
#include "wx_util.h"
#include "image_sequence_dialog.h"

using libdcp::raw_convert;

ImageSequenceDialog::ImageSequenceDialog (wxWindow* parent)
	: TableDialog (parent, _("Add image sequence"), 2, true)
{
	add (_("Frame rate"), true);
	_frame_rate = add (new wxTextCtrl (this, wxID_ANY, N_("24")));
	_digest = new wxCheckBox (this, wxID_ANY, _("Calculate digests"));
	_digest->SetValue (true);
	_digest->SetToolTip (_("By default DCP-o-matic will calculate digests (hashes) of your image files so that it knows if they change.  Turning this off will speed up import but you must not alter the image files after import or strange things may happen."));
	add (_digest);

	layout ();
}

float
ImageSequenceDialog::frame_rate () const
{
	try {
		return raw_convert<float> (wx_to_std (_frame_rate->GetValue ()));
	} catch (...) {

	}

	return 0;
}

bool
ImageSequenceDialog::digest () const
{
	return _digest->GetValue ();
}
