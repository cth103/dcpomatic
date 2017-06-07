/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "export_dialog.h"
#include "file_picker_ctrl.h"
#include "wx_util.h"
#include <boost/bind.hpp>

using boost::bind;

#define FORMATS 2

wxString format_names[] = {
	_("ProRes"),
	_("H.264")
};

wxString format_filters[] = {
	_("MOV files (*.mov)|*.mov"),
	_("MP4 files (*.mp4)|*.mp4"),
};

FFmpegEncoder::Format formats[] = {
	FFmpegEncoder::FORMAT_PRORES,
	FFmpegEncoder::FORMAT_H264,
};

ExportDialog::ExportDialog (wxWindow* parent)
	: TableDialog (parent, _("Export film"), 2, 1, true)
{
	add (_("Format"), true);
	_format = new wxChoice (this, wxID_ANY);
	add (_format);
	add_spacer ();
	_mixdown = new wxCheckBox (this, wxID_ANY, _("Mix audio down to stereo"));
	add (_mixdown, false);
	add (_("Output file"), true);
	_file = new FilePickerCtrl (this, _("Select output file"), format_filters[0], false);
	add (_file);

	for (int i = 0; i < FORMATS; ++i) {
		_format->Append (format_names[i]);
	}
	_format->SetSelection (0);

	_format->Bind (wxEVT_CHOICE, bind (&ExportDialog::format_changed, this));

	layout ();
}

void
ExportDialog::format_changed ()
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	_file->SetWildcard (format_filters[_format->GetSelection()]);
	_file->SetPath ("");
}

boost::filesystem::path
ExportDialog::path () const
{
	return wx_to_std (_file->GetPath ());
}

FFmpegEncoder::Format
ExportDialog::format () const
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	return formats[_format->GetSelection()];
}

bool
ExportDialog::mixdown_to_stereo () const
{
	return _mixdown->GetValue ();
}
