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


#include "table_dialog.h"
#include "timecode.h"


class PlayheadToTimecodeDialog : public TableDialog
{
public:
	PlayheadToTimecodeDialog (wxWindow* parent, dcpomatic::DCPTime time, int fps);

	dcpomatic::DCPTime get () const;

private:
	Timecode<dcpomatic::DCPTime>* _timecode;
	int _fps;
};
