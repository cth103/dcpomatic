/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "filter_dialog.h"
#include "video_panel.h"
#include "wx_util.h"
#include "content_colour_conversion_dialog.h"
#include "content_widget.h"
#include "content_panel.h"
#include "lib/filter.h"
#include "lib/ffmpeg_content.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/ratio.h"
#include "lib/frame_rate_change.h"
#include "lib/dcp_content.h"
#include "lib/video_content.h"
#include <wx/spinctrl.h>
#include <boost/foreach.hpp>
#include <set>
#include <iostream>

using std::vector;
using std::string;
using std::pair;
using std::cout;
using std::list;
using std::set;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using boost::optional;

static VideoContentScale
index_to_scale (int n)
{
	vector<VideoContentScale> scales = VideoContentScale::all ();
	DCPOMATIC_ASSERT (n >= 0);
	DCPOMATIC_ASSERT (n < int (scales.size ()));
	return scales[n];
}

static int
scale_to_index (VideoContentScale scale)
{
	vector<VideoContentScale> scales = VideoContentScale::all ();
	for (size_t i = 0; i < scales.size(); ++i) {
		if (scales[i] == scale) {
			return i;
		}
	}

	DCPOMATIC_ASSERT (false);
}

VideoPanel::VideoPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Video"))
{
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;

	_reference = new wxCheckBox (this, wxID_ANY, _("Refer to existing DCP"));
	grid->Add (_reference, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_sizer (grid, this, _("Type"), true, wxGBPosition (r, 0));
	_frame_type = new ContentChoice<VideoContent, VideoFrameType> (
		this,
		new wxChoice (this, wxID_ANY),
		VideoContentProperty::VIDEO_FRAME_TYPE,
		boost::mem_fn (&VideoContent::video_frame_type),
		boost::mem_fn (&VideoContent::set_video_frame_type),
		&caster<int, VideoFrameType>,
		&caster<VideoFrameType, int>
		);
	_frame_type->add (grid, wxGBPosition (r, 1), wxGBSpan (1, 2));
	++r;

	add_label_to_sizer (grid, this, _("Crop"), true, wxGBPosition (r, 0));

	int cr = 0;
	wxGridBagSizer* crop = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_sizer (crop, this, _("Left"), true, wxGBPosition (cr, 0));
	_left_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::left_crop),
		boost::mem_fn (&VideoContent::set_left_crop)
		);
	_left_crop->add (crop, wxGBPosition (cr, 1));

	add_label_to_sizer (crop, this, _("Right"), true, wxGBPosition (cr, 2));
	_right_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::right_crop),
		boost::mem_fn (&VideoContent::set_right_crop)
		);
	_right_crop->add (crop, wxGBPosition (cr, 3));

	++cr;

	add_label_to_sizer (crop, this, _("Top"), true, wxGBPosition (cr, 0));
	_top_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::top_crop),
		boost::mem_fn (&VideoContent::set_top_crop)
		);
	_top_crop->add (crop, wxGBPosition (cr, 1));

	add_label_to_sizer (crop, this, _("Bottom"), true, wxGBPosition (cr, 2));
	_bottom_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::bottom_crop),
		boost::mem_fn (&VideoContent::set_bottom_crop)
		);
	_bottom_crop->add (crop, wxGBPosition (cr, 3));

	grid->Add (crop, wxGBPosition (r, 1), wxGBSpan (2, 3));
	r += 2;

	add_label_to_sizer (grid, this, _("Fade in"), true, wxGBPosition (r, 0));
	_fade_in = new Timecode<ContentTime> (this);
	grid->Add (_fade_in, wxGBPosition (r, 1), wxGBSpan (1, 3));
	++r;

	add_label_to_sizer (grid, this, _("Fade out"), true, wxGBPosition (r, 0));
	_fade_out = new Timecode<ContentTime> (this);
	grid->Add (_fade_out, wxGBPosition (r, 1), wxGBSpan (1, 3));
	++r;

	add_label_to_sizer (grid, this, _("Scale to"), true, wxGBPosition (r, 0));
	_scale = new ContentChoice<VideoContent, VideoContentScale> (
		this,
		new wxChoice (this, wxID_ANY),
		VideoContentProperty::VIDEO_SCALE,
		boost::mem_fn (&VideoContent::scale),
		boost::mem_fn (&VideoContent::set_scale),
		&index_to_scale,
		&scale_to_index
		);
	_scale->add (grid, wxGBPosition (r, 1), wxGBSpan (1, 2));
	++r;

	wxClientDC dc (this);
	wxSize size = dc.GetTextExtent (wxT ("A quite long name"));
	size.SetHeight (-1);

	add_label_to_sizer (grid, this, _("Filters"), true, wxGBPosition (r, 0));
	{
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		_filters = new wxStaticText (this, wxID_ANY, _("None"), wxDefaultPosition, size);
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_filters_button, 0, wxALIGN_CENTER_VERTICAL);

		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;

	add_label_to_sizer (grid, this, _("Colour conversion"), true, wxGBPosition (r, 0));
	{
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		_colour_conversion = new wxChoice (this, wxID_ANY, wxDefaultPosition, size);
		_colour_conversion->Append (_("None"));
		BOOST_FOREACH (PresetColourConversion const & i, PresetColourConversion::all()) {
			_colour_conversion->Append (std_to_wx (i.name));
		}

		/// TRANSLATORS: translate the word "Custom" here; do not include the "Colour|" prefix
		_colour_conversion->Append (S_("Colour|Custom"));
		s->Add (_colour_conversion, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);

		_edit_colour_conversion_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_edit_colour_conversion_button, 0, wxALIGN_CENTER_VERTICAL);

		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;

	_description = new wxStaticText (this, wxID_ANY, wxT ("\n \n \n \n \n"), wxDefaultPosition, wxDefaultSize);
	grid->Add (_description, wxGBPosition (r, 0), wxGBSpan (1, 4), wxEXPAND | wxALIGN_CENTER_VERTICAL, 6);
	wxFont font = _description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_description->SetFont(font);
	++r;

	_left_crop->wrapped()->SetRange (0, 1024);
	_top_crop->wrapped()->SetRange (0, 1024);
	_right_crop->wrapped()->SetRange (0, 1024);
	_bottom_crop->wrapped()->SetRange (0, 1024);

	_scale->wrapped()->Clear ();
	BOOST_FOREACH (VideoContentScale const & i, VideoContentScale::all ()) {
		_scale->wrapped()->Append (std_to_wx (i.name ()));
	}

	_frame_type->wrapped()->Append (_("2D"));
	_frame_type->wrapped()->Append (_("3D left/right"));
	_frame_type->wrapped()->Append (_("3D top/bottom"));
	_frame_type->wrapped()->Append (_("3D alternate"));
	_frame_type->wrapped()->Append (_("3D left only"));
	_frame_type->wrapped()->Append (_("3D right only"));

	_fade_in->Changed.connect (boost::bind (&VideoPanel::fade_in_changed, this));
	_fade_out->Changed.connect (boost::bind (&VideoPanel::fade_out_changed, this));

	_reference->Bind                     (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&VideoPanel::reference_clicked, this));
	_filters_button->Bind                (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_filters_clicked, this));
	_colour_conversion->Bind             (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&VideoPanel::colour_conversion_changed, this));
	_edit_colour_conversion_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_colour_conversion_clicked, this));
}

void
VideoPanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::CONTAINER:
	case Film::VIDEO_FRAME_RATE:
	case Film::RESOLUTION:
		setup_description ();
		break;
	case Film::REEL_TYPE:
		setup_sensitivity ();
	default:
		break;
	}
}

void
VideoPanel::film_content_changed (int property)
{
	ContentList vc = _parent->selected_video ();
	shared_ptr<Content> vcs;
	shared_ptr<FFmpegContent> fcs;
	if (!vc.empty ()) {
		vcs = vc.front ();
		fcs = dynamic_pointer_cast<FFmpegContent> (vcs);
	}

	if (property == VideoContentProperty::VIDEO_FRAME_TYPE) {
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_CROP) {
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_SCALE) {
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_FRAME_RATE) {
		setup_description ();
	} else if (property == VideoContentProperty::COLOUR_CONVERSION) {
		if (vcs && vcs->video->colour_conversion ()) {
			optional<size_t> preset = vcs->video->colour_conversion().get().preset ();
			vector<PresetColourConversion> cc = PresetColourConversion::all ();
			if (preset) {
				checked_set (_colour_conversion, preset.get() + 1);
			} else {
				checked_set (_colour_conversion, cc.size() + 1);
			}
		} else {
			checked_set (_colour_conversion, 0);
		}

		setup_sensitivity ();

	} else if (property == FFmpegContentProperty::FILTERS) {
		if (fcs) {
			string p = Filter::ffmpeg_string (fcs->filters ());
			if (p.empty ()) {
				checked_set (_filters, _("None"));
			} else {
				if (p.length() > 25) {
					p = p.substr (0, 25) + "...";
				}
				checked_set (_filters, p);
			}
		}
	} else if (property == VideoContentProperty::VIDEO_FADE_IN) {
		set<Frame> check;
		BOOST_FOREACH (shared_ptr<const Content> i, vc) {
			check.insert (i->video->fade_in ());
		}

		if (check.size() == 1) {
			_fade_in->set (
				ContentTime::from_frames (vc.front()->video->fade_in (), vc.front()->video->video_frame_rate ()),
				vc.front()->video->video_frame_rate ()
				);
		} else {
			_fade_in->clear ();
		}
	} else if (property == VideoContentProperty::VIDEO_FADE_OUT) {
		set<Frame> check;
		BOOST_FOREACH (shared_ptr<const Content> i, vc) {
			check.insert (i->video->fade_out ());
		}

		if (check.size() == 1) {
			_fade_out->set (
				ContentTime::from_frames (vc.front()->video->fade_out (), vc.front()->video->video_frame_rate ()),
				vc.front()->video->video_frame_rate ()
				);
		} else {
			_fade_out->clear ();
		}
	} else if (property == DCPContentProperty::REFERENCE_VIDEO) {
		if (vc.size() == 1) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (vc.front ());
			checked_set (_reference, dcp ? dcp->reference_video () : false);
		} else {
			checked_set (_reference, false);
		}

		setup_sensitivity ();
	}
}

/** Called when the `Edit filters' button has been clicked */
void
VideoPanel::edit_filters_clicked ()
{
	FFmpegContentList c = _parent->selected_ffmpeg ();
	if (c.size() != 1) {
		return;
	}

	FilterDialog* d = new FilterDialog (this, c.front()->filters());
	d->ActiveChanged.connect (bind (&FFmpegContent::set_filters, c.front(), _1));
	d->ShowModal ();
	d->Destroy ();
}

void
VideoPanel::setup_description ()
{
	ContentList vc = _parent->selected_video ();
	if (vc.empty ()) {
		checked_set (_description, wxT (""));
		return;
	} else if (vc.size() > 1) {
		checked_set (_description, _("Multiple content selected"));
		return;
	}

	string d = vc.front()->video->processing_description ();
	size_t lines = count (d.begin(), d.end(), '\n');

	for (int i = lines; i < 6; ++i) {
		d += "\n ";
	}

	checked_set (_description, d);
	_sizer->Layout ();
}

void
VideoPanel::colour_conversion_changed ()
{
	ContentList vc = _parent->selected_video ();
	if (vc.size() != 1) {
		return;
	}

	int const s = _colour_conversion->GetSelection ();
	vector<PresetColourConversion> all = PresetColourConversion::all ();

	if (s == 0) {
		vc.front()->video->unset_colour_conversion ();
	} else if (s == int (all.size() + 1)) {
		edit_colour_conversion_clicked ();
	} else {
		vc.front()->video->set_colour_conversion (all[s - 1].conversion);
	}
}

void
VideoPanel::edit_colour_conversion_clicked ()
{
	ContentList vc = _parent->selected_video ();
	if (vc.size() != 1) {
		return;
	}

	ContentColourConversionDialog* d = new ContentColourConversionDialog (this, vc.front()->yuv ());
	d->set (vc.front()->video->colour_conversion().get_value_or (PresetColourConversion::all().front ().conversion));
	d->ShowModal ();
	vc.front()->video->set_colour_conversion (d->get ());
	d->Destroy ();
}

void
VideoPanel::content_selection_changed ()
{
	ContentList video_sel = _parent->selected_video ();

	_frame_type->set_content (video_sel);
	_left_crop->set_content (video_sel);
	_right_crop->set_content (video_sel);
	_top_crop->set_content (video_sel);
	_bottom_crop->set_content (video_sel);
	_scale->set_content (video_sel);

	film_content_changed (VideoContentProperty::VIDEO_CROP);
	film_content_changed (VideoContentProperty::VIDEO_FRAME_RATE);
	film_content_changed (VideoContentProperty::COLOUR_CONVERSION);
	film_content_changed (VideoContentProperty::VIDEO_FADE_IN);
	film_content_changed (VideoContentProperty::VIDEO_FADE_OUT);
	film_content_changed (FFmpegContentProperty::FILTERS);
	film_content_changed (DCPContentProperty::REFERENCE_VIDEO);

	setup_sensitivity ();
}

void
VideoPanel::setup_sensitivity ()
{
	ContentList sel = _parent->selected ();

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent> (sel.front ());
	}

	list<string> why_not;
	bool const can_reference = dcp && dcp->can_reference_video (why_not);
	setup_refer_button (_reference, dcp, can_reference, why_not);

	if (_reference->GetValue ()) {
		_frame_type->wrapped()->Enable (false);
		_left_crop->wrapped()->Enable (false);
		_right_crop->wrapped()->Enable (false);
		_top_crop->wrapped()->Enable (false);
		_bottom_crop->wrapped()->Enable (false);
		_fade_in->Enable (false);
		_fade_out->Enable (false);
		_scale->wrapped()->Enable (false);
		_description->Enable (false);
		_filters->Enable (false);
		_filters_button->Enable (false);
		_colour_conversion->Enable (false);
	} else {
		ContentList video_sel = _parent->selected_video ();
		FFmpegContentList ffmpeg_sel = _parent->selected_ffmpeg ();
		bool const single = video_sel.size() == 1;

		_frame_type->wrapped()->Enable (true);
		_left_crop->wrapped()->Enable (true);
		_right_crop->wrapped()->Enable (true);
		_top_crop->wrapped()->Enable (true);
		_bottom_crop->wrapped()->Enable (true);
		_fade_in->Enable (!video_sel.empty ());
		_fade_out->Enable (!video_sel.empty ());
		_scale->wrapped()->Enable (true);
		_description->Enable (true);
		_filters->Enable (true);
		_filters_button->Enable (single && !ffmpeg_sel.empty ());
		_colour_conversion->Enable (single && !video_sel.empty ());
	}

	ContentList vc = _parent->selected_video ();
	shared_ptr<Content> vcs;
	if (!vc.empty ()) {
		vcs = vc.front ();
	}

	if (vcs && vcs->video->colour_conversion ()) {
		_edit_colour_conversion_button->Enable (!vcs->colour_conversion().get().preset());
	} else {
		_edit_colour_conversion_button->Enable (false);
	}
}

void
VideoPanel::fade_in_changed ()
{
	BOOST_FOREACH (shared_ptr<VideoContent> i, _parent->selected_video ()) {
		int const vfr = _parent->film()->video_frame_rate ();
		i->set_fade_in (_fade_in->get (vfr).frames_round (vfr));
	}
}

void
VideoPanel::fade_out_changed ()
{
	BOOST_FOREACH (shared_ptr<VideoContent> i, _parent->selected_video ()) {
		int const vfr = _parent->film()->video_frame_rate ();
		i->set_fade_out (_fade_out->get (vfr).frames_round (vfr));
	}
}

void
VideoPanel::reference_clicked ()
{
	ContentList c = _parent->selected ();
	if (c.size() != 1) {
		return;
	}

	shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (c.front ());
	if (!d) {
		return;
	}

	d->set_reference_video (_reference->GetValue ());
}
