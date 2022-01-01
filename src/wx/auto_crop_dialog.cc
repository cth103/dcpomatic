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


#include "auto_crop_dialog.h"
#include "dcpomatic_spin_ctrl.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/types.h"


AutoCropDialog::AutoCropDialog (wxWindow* parent, Crop crop)
	: TableDialog (parent, _("Auto crop"), 2, 1, true)
{
	add (_("Left"), true);
	_left = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH));
	add (_("Right"), true);
	_right = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH));
	add (_("Top"), true);
	_top = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH));
	add (_("Bottom"), true);
	_bottom = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH));
	add (_("Threshold"), true);
	_threshold = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH));

	_left->SetRange(0, 4096);
	_right->SetRange(0, 4096);
	_top->SetRange(0, 4096);
	_bottom->SetRange(0, 4096);

	set (crop);
	_threshold->SetValue (std::round(Config::instance()->auto_crop_threshold() * 100));

	layout ();

	_left->Bind (wxEVT_SPINCTRL, [this](wxSpinEvent&) { Changed(get()); });
	_right->Bind (wxEVT_SPINCTRL, [this](wxSpinEvent&) { Changed(get()); });
	_top->Bind (wxEVT_SPINCTRL, [this](wxSpinEvent&) { Changed(get()); });
	_bottom->Bind (wxEVT_SPINCTRL, [this](wxSpinEvent&) { Changed(get()); });
	_threshold->Bind (wxEVT_SPINCTRL, [](wxSpinEvent& ev) { Config::instance()->set_auto_crop_threshold(ev.GetPosition() / 100.0); });
}


Crop
AutoCropDialog::get () const
{
	return Crop(_left->GetValue(), _right->GetValue(), _top->GetValue(), _bottom->GetValue());
}


void
AutoCropDialog::set (Crop crop)
{
	_left->SetValue (crop.left);
	_right->SetValue (crop.right);
	_top->SetValue (crop.top);
	_bottom->SetValue (crop.bottom);
}

