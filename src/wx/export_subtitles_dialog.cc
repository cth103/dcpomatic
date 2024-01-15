/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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
#include "export_subtitles_dialog.h"
#include "file_picker_ctrl.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


using std::string;
using boost::bind;


ExportSubtitlesDialog::ExportSubtitlesDialog (wxWindow* parent, int reels, bool interop)
	: TableDialog (parent, _("Export subtitles"), 2, 1, true)
	, _interop (interop)
	, _include_font (0)
{
	_split_reels = new CheckBox (this, _("Write reels into separate files"));
	add (_split_reels, false);
	add_spacer ();

	if (reels > 1) {
		_split_reels->Enable (false);
	}

	_include_font = new CheckBox (this, _("Define font in output and export font file"));
	add (_include_font, false);
	add_spacer ();

	if (!_interop) {
		_include_font->Enable (false);
	}

	wxString const wildcard = _interop ? _("Subtitle files (.xml)|*.xml") : _("Subtitle files (.mxf)|*.mxf");

	_file_label = add (_("Output file"), true);
	_file = new FilePickerCtrl (this, _("Select output file"), wildcard, false, true);
	add (_file);

	_dir_label = add (_("Output folder"), true);
	_dir = new DirPickerCtrl (this);
	add (_dir);

	_split_reels->bind(&ExportSubtitlesDialog::setup_sensitivity, this);
	_include_font->bind(&ExportSubtitlesDialog::setup_sensitivity, this);
	_file->Bind (wxEVT_FILEPICKER_CHANGED, bind(&ExportSubtitlesDialog::setup_sensitivity, this));
	_dir->Bind (wxEVT_DIRPICKER_CHANGED, bind(&ExportSubtitlesDialog::setup_sensitivity, this));

	layout ();
	setup_sensitivity ();
}


void
ExportSubtitlesDialog::setup_sensitivity ()
{
	bool const multi = split_reels() || (_interop && _include_font->GetValue());
	_file_label->Enable (!multi);
	_file->Enable (!multi);
	_dir_label->Enable (multi);
	_dir->Enable (multi);

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById(wxID_OK, this));
	DCPOMATIC_ASSERT (ok);
	ok->Enable (path().is_absolute());
}


/** @return Either a full path to a file, if the output will be one file, or
 *  a full path to a directory.
 */
boost::filesystem::path
ExportSubtitlesDialog::path () const
{
	if (_file->IsEnabled()) {
		wxFileName fn(std_to_wx(_file->path().string()));
		fn.SetExt (_interop ? "xml" : "mxf");
		return wx_to_std (fn.GetFullPath());
	}

	return wx_to_std (_dir->GetPath());
}


bool
ExportSubtitlesDialog::split_reels () const
{
	return _split_reels->GetValue ();
}


bool
ExportSubtitlesDialog::include_font () const
{
	return !_interop || _include_font->GetValue();
}

