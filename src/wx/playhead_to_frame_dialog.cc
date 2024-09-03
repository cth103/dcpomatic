/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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

#include "playhead_to_frame_dialog.h"
#include <dcp/locale_convert.h>

using std::string;
using dcp::locale_convert;
using namespace dcpomatic;

PlayheadToFrameDialog::PlayheadToFrameDialog (wxWindow* parent, DCPTime time, int fps)
	: TableDialog (parent, _("Go to frame"), 2, 1, true)
	, _fps (fps)
{
	add (_("Go to"), true);
	_frame = add(new wxTextCtrl(this, wxID_ANY, {}));
	_frame->SetFocus ();
	_frame->SetValue (std_to_wx(locale_convert<string>(time.frames_round(fps) + 1)));
	_frame->SetSelection (-1, -1);

	layout ();
}

DCPTime
PlayheadToFrameDialog::get () const
{
	return DCPTime::from_frames (locale_convert<Frame> (wx_to_std (_frame->GetValue ())) - 1, _fps);
}
