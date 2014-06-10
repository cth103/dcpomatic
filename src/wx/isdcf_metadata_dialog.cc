/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include "lib/film.h"
#include "isdcf_metadata_dialog.h"
#include "wx_util.h"

using boost::shared_ptr;

ISDCFMetadataDialog::ISDCFMetadataDialog (wxWindow* parent, ISDCFMetadata dm)
	: TableDialog (parent, _("ISDCF name"), 2, true)
{
	add (_("Content version"), true);
	_content_version = add (new wxSpinCtrl (this, wxID_ANY));

	add (_("Audio Language (e.g. EN)"), true);
	_audio_language = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Subtitle Language (e.g. FR)"), true);
	_subtitle_language = add (new wxTextCtrl (this, wxID_ANY));
	
	add (_("Territory (e.g. UK)"), true);
	_territory = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Rating (e.g. 15)"), true);
	_rating = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Studio (e.g. TCF)"), true);
	_studio = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Facility (e.g. DLA)"), true);
	_facility = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Package Type (e.g. OV)"), true);
	_package_type = add (new wxTextCtrl (this, wxID_ANY));

	_content_version->SetRange (1, 1024);

	_content_version->SetValue (dm.content_version);
	_audio_language->SetValue (std_to_wx (dm.audio_language));
	_subtitle_language->SetValue (std_to_wx (dm.subtitle_language));
	_territory->SetValue (std_to_wx (dm.territory));
	_rating->SetValue (std_to_wx (dm.rating));
	_studio->SetValue (std_to_wx (dm.studio));
	_facility->SetValue (std_to_wx (dm.facility));
	_package_type->SetValue (std_to_wx (dm.package_type));

	layout ();
}

ISDCFMetadata
ISDCFMetadataDialog::isdcf_metadata () const
{
	ISDCFMetadata dm;

	dm.content_version = _content_version->GetValue ();
	dm.audio_language = wx_to_std (_audio_language->GetValue ());
	dm.subtitle_language = wx_to_std (_subtitle_language->GetValue ());
	dm.territory = wx_to_std (_territory->GetValue ());
	dm.rating = wx_to_std (_rating->GetValue ());
	dm.studio = wx_to_std (_studio->GetValue ());
	dm.facility = wx_to_std (_facility->GetValue ());
	dm.package_type = wx_to_std (_package_type->GetValue ());

	return dm;
}
