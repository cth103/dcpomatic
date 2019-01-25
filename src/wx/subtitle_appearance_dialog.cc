/*
    Copyright (C) 2015-2018 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_appearance_dialog.h"
#include "rgba_colour_picker.h"
#include "static_text.h"
#include "check_box.h"
#include "dcpomatic_button.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/examine_ffmpeg_subtitles_job.h"
#include "lib/job_manager.h"
#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

using std::map;
using std::string;
using boost::shared_ptr;
using boost::bind;
using boost::dynamic_pointer_cast;
using boost::optional;

int const SubtitleAppearanceDialog::NONE = 0;
int const SubtitleAppearanceDialog::OUTLINE = 1;
int const SubtitleAppearanceDialog::SHADOW = 2;

SubtitleAppearanceDialog::SubtitleAppearanceDialog (wxWindow* parent, shared_ptr<const Film> film, shared_ptr<Content> content, shared_ptr<TextContent> caption)
	: wxDialog (parent, wxID_ANY, _("Caption appearance"))
	, _film (film)
	, _finding (0)
	, _content (content)
	, _caption (caption)
{
	shared_ptr<FFmpegContent> ff = dynamic_pointer_cast<FFmpegContent> (content);
	if (ff) {
		_stream = ff->subtitle_stream ();
		/* XXX: assuming that all FFmpeg streams have bitmap subs */
		if (_stream->colours().empty()) {
			_job_manager_connection = JobManager::instance()->ActiveJobsChanged.connect(boost::bind(&SubtitleAppearanceDialog::active_jobs_changed, this, _1));
			JobManager::instance()->add(shared_ptr<Job>(new ExamineFFmpegSubtitlesJob(film, ff)));
		}
	}

	_overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_overall_sizer);

	_table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	_overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	int r = 0;

	add_label_to_sizer (_table, this, _("Colour"), true, wxGBPosition (r, 0));
	_force_colour = set_to (_colour = new wxColourPickerCtrl (this, wxID_ANY), r);

	add_label_to_sizer (_table, this, _("Effect"), true, wxGBPosition (r, 0));
	_force_effect = set_to (_effect = new wxChoice (this, wxID_ANY), r);

	add_label_to_sizer (_table, this, _("Effect colour"), true, wxGBPosition (r, 0));
	_force_effect_colour = set_to (_effect_colour = new wxColourPickerCtrl (this, wxID_ANY), r);

	add_label_to_sizer (_table, this, _("Outline width"), true, wxGBPosition (r, 0));
	_outline_width = new wxSpinCtrl (this, wxID_ANY);
	_table->Add (_outline_width, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Fade in time"), true, wxGBPosition (r, 0));
	_force_fade_in = set_to (_fade_in = new Timecode<ContentTime> (this), r);

	add_label_to_sizer (_table, this, _("Fade out time"), true, wxGBPosition (r, 0));
	_force_fade_out = set_to (_fade_out = new Timecode<ContentTime> (this), r);

	if (_stream) {
		_colours_panel = new wxScrolled<wxPanel> (this);
		_colours_panel->EnableScrolling (false, true);
		_colours_panel->ShowScrollbars (wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
		_colours_panel->SetScrollRate (0, 16);

		_colour_table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_colour_table->AddGrowableCol (1, 1);

		wxStaticText* t = new StaticText (_colours_panel, "");
		t->SetLabelMarkup (_("<b>Original colour</b>"));
		_colour_table->Add (t, 1, wxEXPAND);
		t = new StaticText (_colours_panel, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
		t->SetLabelMarkup (_("<b>New colour</b>"));
		_colour_table->Add (t, 1, wxALIGN_CENTER);

		add_colours ();

		_colours_panel->SetSizer (_colour_table);

		/* XXX: still assuming that all FFmpeg streams have bitmap subs */
		if (_stream->colours().empty()) {
			_finding = new wxStaticText(this, wxID_ANY, _("Finding the colours in these subtitles..."));
			_overall_sizer->Add (_finding, 0, wxALL, DCPOMATIC_DIALOG_BORDER);
			_colours_panel->Show (false);
		}

		_overall_sizer->Add (_colours_panel, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		wxButton* restore = new Button (this, _("Restore to original colours"));
		restore->Bind (wxEVT_BUTTON, bind (&SubtitleAppearanceDialog::restore, this));
		_overall_sizer->Add (restore, 0, wxALL, DCPOMATIC_SIZER_X_GAP);
	}

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		_overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);

	/* Keep these Appends() up to date with NONE/OUTLINE/SHADOW variables */
	_effect->Append (_("None"));
	_effect->Append (_("Outline"));
	_effect->Append (_("Shadow"));;

	optional<dcp::Colour> colour = _caption->colour();
	_force_colour->SetValue (static_cast<bool>(colour));
	if (colour) {
		_colour->SetColour (wxColour (colour->r, colour->g, colour->b));
	} else {
		_colour->SetColour (wxColour (255, 255, 255));
	}

	optional<dcp::Effect> effect = _caption->effect();
	_force_effect->SetValue (static_cast<bool>(effect));
	if (effect) {
		switch (*effect) {
		case dcp::NONE:
			_effect->SetSelection (NONE);
			break;
		case dcp::BORDER:
			_effect->SetSelection (OUTLINE);
			break;
		case dcp::SHADOW:
			_effect->SetSelection (SHADOW);
			break;
		}
	} else {
		_effect->SetSelection (NONE);
	}

	optional<dcp::Colour> effect_colour = _caption->effect_colour();
	_force_effect_colour->SetValue (static_cast<bool>(effect_colour));
	if (effect_colour) {
		_effect_colour->SetColour (wxColour (effect_colour->r, effect_colour->g, effect_colour->b));
	} else {
		_effect_colour->SetColour (wxColour (0, 0, 0));
	}

	optional<ContentTime> fade_in = _caption->fade_in();
	_force_fade_in->SetValue (static_cast<bool>(fade_in));
	if (fade_in) {
		_fade_in->set (*fade_in, _content->active_video_frame_rate(film));
	} else {
		_fade_in->set (ContentTime(), _content->active_video_frame_rate(film));
	}

	optional<ContentTime> fade_out = _caption->fade_out();
	_force_fade_out->SetValue (static_cast<bool>(fade_out));
	if (fade_out) {
		_fade_out->set (*fade_out, _content->active_video_frame_rate(film));
	} else {
		_fade_out->set (ContentTime(), _content->active_video_frame_rate(film));
	}

	_outline_width->SetValue (_caption->outline_width ());

	_force_colour->Bind (wxEVT_CHECKBOX, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_force_effect_colour->Bind (wxEVT_CHECKBOX, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_force_effect->Bind (wxEVT_CHECKBOX, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_force_fade_in->Bind (wxEVT_CHECKBOX, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_force_fade_out->Bind (wxEVT_CHECKBOX, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_effect->Bind (wxEVT_CHOICE, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_content_connection = _content->Change.connect (bind (&SubtitleAppearanceDialog::content_change, this, _1));

	setup_sensitivity ();
}

void
SubtitleAppearanceDialog::content_change (ChangeType type)
{
	if (type == CHANGE_TYPE_DONE) {
		setup_sensitivity ();
	}
}

wxCheckBox*
SubtitleAppearanceDialog::set_to (wxWindow* w, int& r)
{
	wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	wxCheckBox* set_to = new CheckBox (this, _("Set to"));
	s->Add (set_to, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
	s->Add (w, 0, wxALIGN_CENTER_VERTICAL);
	_table->Add (s, wxGBPosition (r, 1));
	++r;
	return set_to;
}

void
SubtitleAppearanceDialog::apply ()
{
	shared_ptr<const Film> film = _film.lock ();

	if (_force_colour->GetValue ()) {
		wxColour const c = _colour->GetColour ();
		_caption->set_colour (dcp::Colour (c.Red(), c.Green(), c.Blue()));
	} else {
		_caption->unset_colour ();
	}
	if (_force_effect->GetValue()) {
		switch (_effect->GetSelection()) {
		case NONE:
			_caption->set_effect (dcp::NONE);
			break;
		case OUTLINE:
			_caption->set_effect (dcp::BORDER);
			break;
		case SHADOW:
			_caption->set_effect (dcp::SHADOW);
			break;
		}
	} else {
		_caption->unset_effect ();
	}
	if (_force_effect_colour->GetValue ()) {
		wxColour const ec = _effect_colour->GetColour ();
		_caption->set_effect_colour (dcp::Colour (ec.Red(), ec.Green(), ec.Blue()));
	} else {
		_caption->unset_effect_colour ();
	}
	if (_force_fade_in->GetValue ()) {
		_caption->set_fade_in (_fade_in->get(_content->active_video_frame_rate(film)));
	} else {
		_caption->unset_fade_in ();
	}
	if (_force_fade_out->GetValue ()) {
		_caption->set_fade_out (_fade_out->get(_content->active_video_frame_rate(film)));
	} else {
		_caption->unset_fade_out ();
	}
	_caption->set_outline_width (_outline_width->GetValue ());

	if (_stream) {
		for (map<RGBA, RGBAColourPicker*>::const_iterator i = _pickers.begin(); i != _pickers.end(); ++i) {
			_stream->set_colour (i->first, i->second->colour ());
		}
	}

	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (_content);
	if (fc) {
		fc->signal_subtitle_stream_changed ();
	}
}

void
SubtitleAppearanceDialog::restore ()
{
	for (map<RGBA, RGBAColourPicker*>::const_iterator i = _pickers.begin(); i != _pickers.end(); ++i) {
		i->second->set (i->first);
	}
}

void
SubtitleAppearanceDialog::setup_sensitivity ()
{
	_colour->Enable (_force_colour->GetValue ());
	_effect_colour->Enable (_force_effect_colour->GetValue ());
	_effect->Enable (_force_effect->GetValue ());
	_fade_in->Enable (_force_fade_in->GetValue ());
	_fade_out->Enable (_force_fade_out->GetValue ());

	bool const can_outline_width = _effect->GetSelection() == OUTLINE && _caption->burn ();
	_outline_width->Enable (can_outline_width);
	if (can_outline_width) {
		_outline_width->UnsetToolTip ();
	} else {
		_outline_width->SetToolTip (_("Outline width cannot be set unless you are burning in captions"));
	}
}

void
SubtitleAppearanceDialog::active_jobs_changed (optional<string> last)
{
	if (last && *last == "examine_subtitles") {
		_colours_panel->Show (true);
		if (_finding) {
			_finding->Show (false);
		}
		add_colours ();
		_overall_sizer->Layout ();
		_overall_sizer->SetSizeHints (this);
	}
}

void
SubtitleAppearanceDialog::add_colours ()
{
	map<RGBA, RGBA> colours = _stream->colours ();
	for (map<RGBA, RGBA>::const_iterator i = colours.begin(); i != colours.end(); ++i) {
		wxPanel* from = new wxPanel (_colours_panel, wxID_ANY);
		from->SetBackgroundColour (wxColour (i->first.r, i->first.g, i->first.b, i->first.a));
		_colour_table->Add (from, 1, wxEXPAND);
		RGBAColourPicker* to = new RGBAColourPicker (_colours_panel, i->second);
		_colour_table->Add (to, 1, wxEXPAND);
		_pickers[i->first] = to;
	}
}
