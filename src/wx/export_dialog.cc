/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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
#include "check_box.h"
#include <wx/filepicker.h>
#include <boost/bind.hpp>

using boost::bind;

#define FORMATS 2

wxString format_names[] = {
	_("ProRes"),
	_("MP4 / H.264")
};

wxString format_filters[] = {
	_("MOV files (*.mov)|*.mov"),
	_("MP4 files (*.mp4)|*.mp4"),
};

wxString format_extensions[] = {
	"mov",
	"mp4"
};

ExportFormat formats[] = {
	EXPORT_FORMAT_PRORES,
	EXPORT_FORMAT_H264,
};

ExportDialog::ExportDialog (wxWindow* parent)
	: TableDialog (parent, _("Export film"), 2, 1, true)
{
	add (_("Format"), true);
	_format = new wxChoice (this, wxID_ANY);
	add (_format);
	add_spacer ();
	_mixdown = new CheckBox (this, _("Mix audio down to stereo"));
	add (_mixdown, false);
	add_spacer ();
	_split_reels = new CheckBox (this, _("Write reels into separate files"));
	add (_split_reels, false);
	_x264_crf_label[0] = add (_("Quality"), true);
	_x264_crf = new wxSlider (this, wxID_ANY, 23, 0, 51, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
	add (_x264_crf, false);
	add_spacer ();
	_x264_crf_label[1] = add (_("0 is best, 51 is worst"), false);
	wxFont font = _x264_crf_label[1]->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_x264_crf_label[1]->SetFont(font);

	add (_("Output file"), true);
	_file = new FilePickerCtrl (this, _("Select output file"), format_filters[0], false);
	add (_file);

	for (int i = 0; i < FORMATS; ++i) {
		_format->Append (format_names[i]);
	}
	_format->SetSelection (0);

	_x264_crf->Enable (false);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable (false);
	}

	_format->Bind (wxEVT_CHOICE, bind (&ExportDialog::format_changed, this));
	_file->Bind (wxEVT_FILEPICKER_CHANGED, bind (&ExportDialog::file_changed, this));

	layout ();

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (false);
}

void
ExportDialog::format_changed ()
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	_file->SetWildcard (format_filters[_format->GetSelection()]);
	_file->SetPath ("");
	_x264_crf->Enable (_format->GetSelection() == 1);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable (_format->GetSelection() == 1);
	}
}

boost::filesystem::path
ExportDialog::path () const
{
	wxFileName fn (_file->GetPath());
	fn.SetExt (format_extensions[_format->GetSelection()]);
	return wx_to_std (fn.GetFullPath());
}

ExportFormat
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

bool
ExportDialog::split_reels () const
{
	return _split_reels->GetValue ();
}

int
ExportDialog::x264_crf () const
{
	return _x264_crf->GetValue ();
}

void
ExportDialog::file_changed ()
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (true);
}
