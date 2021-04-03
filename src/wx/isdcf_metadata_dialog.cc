/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

#include "isdcf_metadata_dialog.h"
#include "wx_util.h"
#include "check_box.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>

using std::shared_ptr;

/** @param parent Parent window.
 *  @param dm Initial ISDCF metadata.
 *  @param threed true if the film is in 3D.
 */
ISDCFMetadataDialog::ISDCFMetadataDialog (wxWindow* parent, ISDCFMetadata dm)
	: TableDialog (parent, _("ISDCF name"), 2, 1, true)
{
	add (_("Mastered luminance (e.g. 14fl)"), true);
	_mastered_luminance = add (new wxTextCtrl (this, wxID_ANY));

	_mastered_luminance->SetValue (std_to_wx (dm.mastered_luminance));

	layout ();
}


ISDCFMetadata
ISDCFMetadataDialog::isdcf_metadata () const
{
	ISDCFMetadata dm;

	dm.mastered_luminance = wx_to_std (_mastered_luminance->GetValue ());

	return dm;
}
