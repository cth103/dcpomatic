/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include "ratio_picker.h"
#include "show_playlist_entry_dialog.h"
#include "wx_util.h"
#include "lib/show_playlist_entry.h"
#include <wx/wx.h>


using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif



ShowPlaylistEntryDialog::ShowPlaylistEntryDialog(wxWindow* parent, ShowPlaylistEntry entry)
	: TableDialog(parent, _("Playlist item"), 2, 1, true)
	, _entry(entry)
{
	add(_("Name"), true);
	auto name = _entry.name();
#ifdef DCPOMATIC_LINUX
	boost::replace_all(name, "_", "__");
#endif
	add(std_to_wx(name), false);
	add(_("UUID"), true);
	add(std_to_wx(_entry.uuid()), false);
	add(_("Type"), true);
	add(std_to_wx(_entry.kind().name()), false);
	add(_("Encrypted"), true);
	add(_entry.encrypted() ? _("Yes") : _("No"), false);

	_crop = new RatioPicker(this, entry.crop_to_ratio());
	add(_crop->enable_checkbox(), false);
	add(_crop, false);

	layout();

	_crop->Changed.connect(boost::bind(&ShowPlaylistEntryDialog::crop_changed, this, _1));
}


ShowPlaylistEntry
ShowPlaylistEntryDialog::get() const
{
	return _entry;
}


void
ShowPlaylistEntryDialog::crop_changed(optional<float> ratio)
{
	_entry.set_crop_to_ratio(ratio);
}

