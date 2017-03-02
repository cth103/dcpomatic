/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include "lib/film.h"
#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>

using boost::shared_ptr;

/** @param parent Parent window.
 *  @param dm Initial ISDCF metadata.
 *  @param threed true if the film is in 3D.
 */
ISDCFMetadataDialog::ISDCFMetadataDialog (wxWindow* parent, ISDCFMetadata dm, bool threed)
	: TableDialog (parent, _("ISDCF name"), 2, 1, true)
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

	_temp_version = add (new wxCheckBox (this, wxID_ANY, _("Temp version")));
	add_spacer ();

	_pre_release = add (new wxCheckBox (this, wxID_ANY, _("Pre-release")));
	add_spacer ();

	_red_band = add (new wxCheckBox (this, wxID_ANY, _("Red band")));
	add_spacer ();

	add (_("Chain"), true);
	_chain = add (new wxTextCtrl (this, wxID_ANY));

	_two_d_version_of_three_d = add (new wxCheckBox (this, wxID_ANY, _("2D version of content available in 3D")));
	add_spacer ();

	if (threed) {
		_two_d_version_of_three_d->Enable (false);
	}

	add (_("Mastered luminance (e.g. 14fl)"), true);
	_mastered_luminance = add (new wxTextCtrl (this, wxID_ANY));

	_content_version->SetRange (1, 1024);

	_content_version->SetValue (dm.content_version);
	_audio_language->SetValue (std_to_wx (dm.audio_language));
	_subtitle_language->SetValue (std_to_wx (dm.subtitle_language));
	_territory->SetValue (std_to_wx (dm.territory));
	_rating->SetValue (std_to_wx (dm.rating));
	_studio->SetValue (std_to_wx (dm.studio));
	_facility->SetValue (std_to_wx (dm.facility));
	_temp_version->SetValue (dm.temp_version);
	_pre_release->SetValue (dm.pre_release);
	_red_band->SetValue (dm.red_band);
	_chain->SetValue (std_to_wx (dm.chain));
	_two_d_version_of_three_d->SetValue (dm.two_d_version_of_three_d);
	_mastered_luminance->SetValue (std_to_wx (dm.mastered_luminance));

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
	dm.temp_version = _temp_version->GetValue ();
	dm.pre_release = _pre_release->GetValue ();
	dm.red_band = _red_band->GetValue ();
	dm.chain = wx_to_std (_chain->GetValue ());
	dm.two_d_version_of_three_d = _two_d_version_of_three_d->GetValue ();
	dm.mastered_luminance = wx_to_std (_mastered_luminance->GetValue ());

	return dm;
}
