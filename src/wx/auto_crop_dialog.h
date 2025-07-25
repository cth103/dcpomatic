/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_AUTO_CROP_DIALOG_H
#define DCPOMATIC_AUTO_CROP_DIALOG_H


#include "table_dialog.h"
#include "lib/crop.h"
#include <boost/signals2.hpp>


class SpinCtrl;


class AutoCropDialog : public TableDialog
{
public:
	AutoCropDialog(wxWindow* parent, Crop crop);

	Crop get() const;
	void set(Crop crop);

	boost::signals2::signal<void (Crop)> Changed;

private:
	SpinCtrl* _left;
	SpinCtrl* _right;
	SpinCtrl* _top;
	SpinCtrl* _bottom;
	SpinCtrl* _threshold;
};


#endif

