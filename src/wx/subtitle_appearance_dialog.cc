/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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
#include "lib/text_subtitle_content.h"
#include "lib/subtitle_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/ffmpeg_content.h"
#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

using std::map;
using boost::shared_ptr;
using boost::bind;
using boost::dynamic_pointer_cast;

int const SubtitleAppearanceDialog::NONE = 0;
int const SubtitleAppearanceDialog::OUTLINE = 1;
int const SubtitleAppearanceDialog::SHADOW = 2;

SubtitleAppearanceDialog::SubtitleAppearanceDialog (wxWindow* parent, shared_ptr<Content> content)
	: wxDialog (parent, wxID_ANY, _("Subtitle appearance"))
	, _content (content)
{
	shared_ptr<FFmpegContent> ff = dynamic_pointer_cast<FFmpegContent> (content);
	if (ff) {
		_stream = ff->subtitle_stream ();
	}

	wxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	_table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	int r = 0;

	add_label_to_sizer (_table, this, _("Colour"), true, wxGBPosition (r, 0));
	_colour = new wxColourPickerCtrl (this, wxID_ANY);
	_table->Add (_colour, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Effect"), true, wxGBPosition (r, 0));
	_effect = new wxChoice (this, wxID_ANY);
	_table->Add (_effect, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Effect colour"), true, wxGBPosition (r, 0));
	_effect_colour = new wxColourPickerCtrl (this, wxID_ANY);
	_table->Add (_effect_colour, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Outline width"), true, wxGBPosition (r, 0));
	_outline_width = new wxSpinCtrl (this, wxID_ANY);
	_table->Add (_outline_width, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Fade in time"), true, wxGBPosition (r, 0));
	_fade_in = new Timecode<ContentTime> (this);
	_table->Add (_fade_in, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_table, this, _("Fade out time"), true, wxGBPosition (r, 0));
	_fade_out = new Timecode<ContentTime> (this);
	_table->Add (_fade_out, wxGBPosition (r, 1));
	++r;

	if (_stream) {
		wxScrolled<wxPanel>* colours_panel = new wxScrolled<wxPanel> (this);
		colours_panel->EnableScrolling (false, true);
		colours_panel->ShowScrollbars (wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
		colours_panel->SetScrollRate (0, 16);

		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);

		map<RGBA, RGBA> colours = _stream->colours ();

		wxStaticText* t = new wxStaticText (colours_panel, wxID_ANY, "");
		t->SetLabelMarkup (_("<b>Original colour</b>"));
		table->Add (t, 1, wxEXPAND);
		t = new wxStaticText (colours_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
		t->SetLabelMarkup (_("<b>New colour</b>"));
		table->Add (t, 1, wxALIGN_CENTER);

		for (map<RGBA, RGBA>::const_iterator i = colours.begin(); i != colours.end(); ++i) {
			wxPanel* from = new wxPanel (colours_panel, wxID_ANY);
			from->SetBackgroundColour (wxColour (i->first.r, i->first.g, i->first.b, i->first.a));
			table->Add (from, 1, wxEXPAND);
			RGBAColourPicker* to = new RGBAColourPicker (colours_panel, i->second);
			table->Add (to, 1, wxEXPAND);
			_pickers[i->first] = to;
		}

		colours_panel->SetSizer (table);

		overall_sizer->Add (colours_panel, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		wxButton* restore = new wxButton (this, wxID_ANY, _("Restore to original colours"));
		restore->Bind (wxEVT_BUTTON, bind (&SubtitleAppearanceDialog::restore, this));
		overall_sizer->Add (restore, 0, wxALL, DCPOMATIC_SIZER_X_GAP);
	}

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	/* Keep these Appends() up to date with NONE/OUTLINE/SHADOW variables */
	_effect->Append (_("None"));
	_effect->Append (_("Outline"));
	_effect->Append (_("Shadow"));;

	_colour->SetColour (wxColour (_content->subtitle->colour().r, _content->subtitle->colour().g, _content->subtitle->colour().b));
	if (_content->subtitle->outline()) {
		_effect->SetSelection (OUTLINE);
	} else if (_content->subtitle->shadow()) {
		_effect->SetSelection (SHADOW);
	} else {
		_effect->SetSelection (NONE);
	}
	_effect_colour->SetColour (
		wxColour (_content->subtitle->effect_colour().r, _content->subtitle->effect_colour().g, _content->subtitle->effect_colour().b)
		);
	_fade_in->set (_content->subtitle->fade_in(), _content->active_video_frame_rate ());
	_fade_out->set (_content->subtitle->fade_out(), _content->active_video_frame_rate ());
	_outline_width->SetValue (_content->subtitle->outline_width ());

	_effect->Bind (wxEVT_CHOICE, bind (&SubtitleAppearanceDialog::setup_sensitivity, this));
	_content_connection = _content->Changed.connect (bind (&SubtitleAppearanceDialog::setup_sensitivity, this));

	setup_sensitivity ();
}

void
SubtitleAppearanceDialog::apply ()
{
	wxColour const c = _colour->GetColour ();
	_content->subtitle->set_colour (dcp::Colour (c.Red(), c.Green(), c.Blue()));
	_content->subtitle->set_outline (_effect->GetSelection() == OUTLINE);
	_content->subtitle->set_shadow (_effect->GetSelection() == SHADOW);
	wxColour const ec = _effect_colour->GetColour ();
	_content->subtitle->set_effect_colour (dcp::Colour (ec.Red(), ec.Green(), ec.Blue()));
	_content->subtitle->set_fade_in (_fade_in->get (_content->active_video_frame_rate ()));
	_content->subtitle->set_fade_out (_fade_out->get (_content->active_video_frame_rate ()));
	_content->subtitle->set_outline_width (_outline_width->GetValue ());

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
	_effect_colour->Enable (_effect->GetSelection() != NONE);

	bool const can_outline_width = _effect->GetSelection() == OUTLINE && _content->subtitle->burn ();
	_outline_width->Enable (can_outline_width);
	if (can_outline_width) {
		_outline_width->UnsetToolTip ();
	} else {
		_outline_width->SetToolTip (_("Outline width cannot be set unless you are burning in subtitles"));
	}
}
