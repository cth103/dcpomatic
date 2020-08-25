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
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/filepicker.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/bind.hpp>


using std::string;
using boost::bind;


ExportSubtitlesDialog::ExportSubtitlesDialog (wxWindow* parent, string name, bool interop)
	: TableDialog (parent, _("Export subtitles"), 2, 1, true)
	, _initial_name (name)
	, _include_font (0)
{
	_split_reels = new CheckBox (this, _("Write reels into separate files"));
	add (_split_reels, false);
	add_spacer ();
	if (interop) {
		_include_font = new CheckBox (this, _("Define font in output and export font file"));
		add (_include_font, false);
		add_spacer ();
	}

	add (_("Output file"), true);
	/* Don't warn overwrite here, because on Linux (at least) if we specify a filename like foo
	   the wxFileDialog will check that foo exists, but we will add an extension so we actually
	   need to check if foo.mov (or similar) exists.  I can't find a way to make wxWidgets do this,
	   so disable its check and the caller will have to do it themselves.
	*/
	_file = new FilePickerCtrl (this, _("Select output file"), _("Subtitle files (.xml)|*.xml"), false, false);
	_file->SetPath (_initial_name);
	add (_file);

	_file->Bind (wxEVT_FILEPICKER_CHANGED, bind(&ExportSubtitlesDialog::file_changed, this));

	layout ();

	wxButton* ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	ok->Enable (false);
}


boost::filesystem::path
ExportSubtitlesDialog::path () const
{
	wxFileName fn (_file->GetPath());
	fn.SetExt (".xml");
	return wx_to_std (fn.GetFullPath());
}


bool
ExportSubtitlesDialog::split_reels () const
{
	return _split_reels->GetValue ();
}


bool
ExportSubtitlesDialog::include_font () const
{
	return _include_font ? _include_font->GetValue () : true;
}


void
ExportSubtitlesDialog::file_changed ()
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById(wxID_OK, this));
	DCPOMATIC_ASSERT (ok);
	ok->Enable (path().is_absolute());
}
