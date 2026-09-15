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
#include "export_audio_file_dialog.h"
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


int constexpr FORMATS = 2;


static wxString format_names[] = {
	_("16-bit WAV"),
	_("24-bit WAV"),
};

static wxString format_filters[] = {
	_("WAV files (*.wav)|*.wav"),
	_("WAV files (*.wav)|*.wav")
};

static wxString format_extensions[] = {
	char_to_wx("wav"),
	char_to_wx("wav")
};

static ExportFormat formats[] = {
	ExportFormat::WAV_16,
	ExportFormat::WAV_24,
};


ExportAudioFileDialog::ExportAudioFileDialog(wxWindow* parent, string name)
	: TableDialog(parent, _("Export audio file"), 2, 1, true)
	, _initial_name(name)
{
	auto& config = Config::instance()->export_config();

	add(_("Format"), true);
	_format = new wxChoice(this, wxID_ANY);
	add(_format);

	add_spacer();
	_mixdown = new CheckBox(this, _("Mix audio down to stereo"));
	add(_mixdown, false);

	add_spacer();
	_split_reels = new CheckBox(this, _("Write reels into separate files"));
	add(_split_reels, false);

	add(_("Output file"), true);
	/* Don't warn overwrite here, because on Linux (at least) if we specify a filename like foo
	   the wxFileDialog will check that foo exists, but we will add an extension so we actually
	   need to check if foo.mov (or similar) exists.  I can't find a way to make wxWidgets do this,
	   so disable its check and the caller will have to do it themselves.
	*/
	_file = new FilePickerCtrl(this, _("Select output file"), format_filters[0], false, false, "ExportAudioPath", _initial_name);
	add(_file);

	for (int i = 0; i < FORMATS; ++i) {
		_format->Append(format_names[i]);
	}
	for (int i = 0; i < FORMATS; ++i) {
		if (config.audio_format() == formats[i]) {
			_format->SetSelection(i);
		}
	}

	_mixdown->SetValue(config.mixdown_to_stereo());
	_split_reels->SetValue(config.split_reels());

	_mixdown->bind(&ExportAudioFileDialog::mixdown_changed, this);
	_split_reels->bind(&ExportAudioFileDialog::split_reels_changed, this);
	_file->Bind(wxEVT_FILEPICKER_CHANGED, bind(&ExportAudioFileDialog::file_changed, this));

	format_changed();

	layout();

	if (auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this))) {
		ok->Enable(false);
	}
}


void
ExportAudioFileDialog::mixdown_changed()
{
	Config::instance()->export_config().set_mixdown_to_stereo(_mixdown->GetValue());
}


void
ExportAudioFileDialog::split_reels_changed()
{
	Config::instance()->export_config().set_split_reels(_split_reels->GetValue());
}


void
ExportAudioFileDialog::format_changed()
{
	auto const selection = _format->GetSelection();
	DCPOMATIC_ASSERT(selection >= 0 && selection < FORMATS);
	_file->set_wildcard(format_filters[selection]);
	Config::instance()->export_config().set_audio_format(formats[selection]);
}

boost::filesystem::path
ExportAudioFileDialog::path() const
{
	auto path = _file->path();
	DCPOMATIC_ASSERT(path);
	wxFileName fn(std_to_wx(path->string()));
	fn.SetExt(format_extensions[_format->GetSelection()]);
	return wx_to_std(fn.GetFullPath());
}


ExportFormat
ExportAudioFileDialog::format() const
{
	DCPOMATIC_ASSERT(_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	return formats[_format->GetSelection()];
}


bool
ExportAudioFileDialog::mixdown_to_stereo() const
{
	return _mixdown->GetValue();
}


bool
ExportAudioFileDialog::split_reels() const
{
	return _split_reels->GetValue();
}


void
ExportAudioFileDialog::file_changed()
{
	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	DCPOMATIC_ASSERT(ok);
	ok->Enable(path().is_absolute());
}
