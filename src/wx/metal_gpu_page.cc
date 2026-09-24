/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "metal_gpu_page.h"
#include "wx_util.h"
#include "lib/config.h"


MetalGPUPage::MetalGPUPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}


wxString
MetalGPUPage::GetName() const
{
	return _("GPU");
}


wxBitmap
MetalGPUPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("gpu"), wxBITMAP_TYPE_PNG);
}


void
MetalGPUPage::setup()
{
	_enable_gpu = new CheckBox(_panel, _("Enable Metal acceleration"));
	_panel->GetSizer()->Add(_enable_gpu, 0, wxALL | wxEXPAND, _border);

	_enable_gpu->bind(&MetalGPUPage::enable_gpu_changed, this);
}


void
MetalGPUPage::config_changed()
{
	checked_set(_enable_gpu, Config::instance()->enable_metal());
}


void
MetalGPUPage::enable_gpu_changed()
{
	Config::instance()->set_enable_metal(_enable_gpu->GetValue());
}

