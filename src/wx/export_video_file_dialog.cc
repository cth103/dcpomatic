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
#include "export_video_file_dialog.h"
#include "file_picker_ctrl.h"
#include "lib/ffmpeg_file_encoder.h"
#include "wx_util.h"
#include "lib/config.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


using std::string;
using boost::bind;


int constexpr FORMATS = 4;


wxString format_names[] = {
	_("MOV / ProRes 4444"),
	_("MOV / ProRes HQ"),
	_("MOV / ProRes LT"),
	_("MP4 / H.264"),
};

wxString format_filters[] = {
	_("MOV files (*.mov)|*.mov"),
	_("MOV files (*.mov)|*.mov"),
	_("MOV files (*.mov)|*.mov"),
	_("MP4 files (*.mp4)|*.mp4"),
};

wxString format_extensions[] = {
	"mov",
	"mov",
	"mov",
	"mp4",
};

ExportFormat formats[] = {
	ExportFormat::PRORES_4444,
	ExportFormat::PRORES_HQ,
	ExportFormat::PRORES_LT,
	ExportFormat::H264_AAC,
};

ExportVideoFileDialog::ExportVideoFileDialog (wxWindow* parent, string name)
	: TableDialog (parent, _("Export video file"), 2, 1, true)
	, _initial_name (name)
{
	auto& config = Config::instance()->export_config();

	add (_("Format"), true);
	_format = new wxChoice (this, wxID_ANY);
	add (_format);
	add_spacer ();
	_mixdown = new CheckBox (this, _("Mix audio down to stereo"));
	add (_mixdown, false);
	add_spacer ();
	_split_reels = new CheckBox (this, _("Write reels into separate files"));
	add (_split_reels, false);
	add_spacer ();
	_split_streams = new CheckBox (this, _("Write each audio channel to its own stream"));
	add (_split_streams, false);
	_x264_crf_label[0] = add (_("Quality"), true);
	_x264_crf = new wxSlider (this, wxID_ANY, config.x264_crf(), 0, 51, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
	add (_x264_crf, false);
	add_spacer ();
	_x264_crf_label[1] = add (_("0 is best, 51 is worst"), false);
	wxFont font = _x264_crf_label[1]->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_x264_crf_label[1]->SetFont(font);

	add (_("Output file"), true);
	/* Don't warn overwrite here, because on Linux (at least) if we specify a filename like foo
	   the wxFileDialog will check that foo exists, but we will add an extension so we actually
	   need to check if foo.mov (or similar) exists.  I can't find a way to make wxWidgets do this,
	   so disable its check and the caller will have to do it themselves.
	*/
	_file = new FilePickerCtrl(this, _("Select output file"), format_filters[0], false, false, "ExportVideoPath", _initial_name);
	add (_file);

	for (int i = 0; i < FORMATS; ++i) {
		_format->Append (format_names[i]);
	}
	for (int i = 0; i < FORMATS; ++i) {
		if (config.format() == formats[i]) {
			_format->SetSelection(i);
		}
	}

	_mixdown->SetValue(config.mixdown_to_stereo());
	_split_reels->SetValue(config.split_reels());
	_split_streams->SetValue(config.split_streams());

	_x264_crf->Enable (false);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable (false);
	}

	_mixdown->bind(&ExportVideoFileDialog::mixdown_changed, this);
	_split_reels->bind(&ExportVideoFileDialog::split_reels_changed, this);
	_split_streams->bind(&ExportVideoFileDialog::split_streams_changed, this);
	_x264_crf->Bind (wxEVT_SLIDER, bind(&ExportVideoFileDialog::x264_crf_changed, this));
	_format->Bind (wxEVT_CHOICE, bind (&ExportVideoFileDialog::format_changed, this));
	_file->Bind (wxEVT_FILEPICKER_CHANGED, bind (&ExportVideoFileDialog::file_changed, this));

	format_changed ();

	layout ();

	auto ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (false);
}


void
ExportVideoFileDialog::mixdown_changed()
{
	Config::instance()->export_config().set_mixdown_to_stereo(_mixdown->GetValue());
}


void
ExportVideoFileDialog::split_reels_changed()
{
	Config::instance()->export_config().set_split_reels(_split_reels->GetValue());
}


void
ExportVideoFileDialog::split_streams_changed()
{
	Config::instance()->export_config().set_split_streams(_split_streams->GetValue());
}


void
ExportVideoFileDialog::x264_crf_changed()
{
	Config::instance()->export_config().set_x264_crf(_x264_crf->GetValue());
}


void
ExportVideoFileDialog::format_changed ()
{
	auto const selection = _format->GetSelection();
	DCPOMATIC_ASSERT (selection >= 0 && selection < FORMATS);
	_file->set_wildcard(format_filters[selection]);
	_x264_crf->Enable (formats[selection] == ExportFormat::H264_AAC);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable(formats[selection] == ExportFormat::H264_AAC);
	}

	Config::instance()->export_config().set_format(formats[selection]);
}

boost::filesystem::path
ExportVideoFileDialog::path () const
{
	auto path = _file->path();
	DCPOMATIC_ASSERT(path);
	wxFileName fn(std_to_wx(path->string()));
	fn.SetExt (format_extensions[_format->GetSelection()]);
	return wx_to_std (fn.GetFullPath());
}

ExportFormat
ExportVideoFileDialog::format () const
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	return formats[_format->GetSelection()];
}

bool
ExportVideoFileDialog::mixdown_to_stereo () const
{
	return _mixdown->GetValue ();
}

bool
ExportVideoFileDialog::split_reels () const
{
	return _split_reels->GetValue ();
}

bool
ExportVideoFileDialog::split_streams () const
{
	return _split_streams->GetValue ();
}

int
ExportVideoFileDialog::x264_crf () const
{
	return _x264_crf->GetValue ();
}

void
ExportVideoFileDialog::file_changed ()
{
	auto ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	DCPOMATIC_ASSERT (ok);
	ok->Enable (path().is_absolute());
}
