/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "lib/filter.h"
#include "lib/ffmpeg_content.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/ratio.h"
#include "lib/frame_rate_change.h"
#include "filter_dialog.h"
#include "video_panel.h"
#include "wx_util.h"
#include "content_colour_conversion_dialog.h"
#include "content_widget.h"
#include "content_panel.h"
#include <wx/spinctrl.h>
#include <set>

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

	add_label_to_grid_bag_sizer (grid, this, _("Type"), true, wxGBPosition (r, 0));
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

	int cr = 0;
	wxGridBagSizer* crop = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_grid_bag_sizer (crop, this, _("Left crop"), true, wxGBPosition (cr, 0));
	_left_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::left_crop),
		boost::mem_fn (&VideoContent::set_left_crop)
		);
	_left_crop->add (crop, wxGBPosition (cr, 1));

	add_label_to_grid_bag_sizer (crop, this, _("Right crop"), true, wxGBPosition (cr, 2));
	_right_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::right_crop),
		boost::mem_fn (&VideoContent::set_right_crop)
		);
	_right_crop->add (crop, wxGBPosition (cr, 3));

	++cr;
	
	add_label_to_grid_bag_sizer (crop, this, _("Top crop"), true, wxGBPosition (cr, 0));
	_top_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::top_crop),
		boost::mem_fn (&VideoContent::set_top_crop)
		);
	_top_crop->add (crop, wxGBPosition (cr, 1));

	add_label_to_grid_bag_sizer (crop, this, _("Bottom crop"), true, wxGBPosition (cr, 2));
	_bottom_crop = new ContentSpinCtrl<VideoContent> (
		this,
		new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)),
		VideoContentProperty::VIDEO_CROP,
		boost::mem_fn (&VideoContent::bottom_crop),
		boost::mem_fn (&VideoContent::set_bottom_crop)
		);
	_bottom_crop->add (crop, wxGBPosition (cr, 3));

	grid->Add (crop, wxGBPosition (r, 0), wxGBSpan (2, 3));
	r += 2;

	add_label_to_grid_bag_sizer (grid, this, _("Fade in"), true, wxGBPosition (r, 0));
	_fade_in = new Timecode<ContentTime> (this);
	grid->Add (_fade_in, wxGBPosition (r, 1), wxGBSpan (1, 3));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Fade out"), true, wxGBPosition (r, 0));
	_fade_out = new Timecode<ContentTime> (this);
	grid->Add (_fade_out, wxGBPosition (r, 1), wxGBSpan (1, 3));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Scale to"), true, wxGBPosition (r, 0));
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
	
	add_label_to_grid_bag_sizer (grid, this, _("Filters"), true, wxGBPosition (r, 0));
	_filters = new wxStaticText (this, wxID_ANY, _("None"), wxDefaultPosition, size);
	grid->Add (_filters, wxGBPosition (r, 1), wxGBSpan (1, 2), wxALIGN_CENTER_VERTICAL);
	_filters_button = new wxButton (this, wxID_ANY, _("Edit..."));
	grid->Add (_filters_button, wxGBPosition (r, 3), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;
	
	_enable_colour_conversion = new wxCheckBox (this, wxID_ANY, _("Colour conversion"));
	grid->Add (_enable_colour_conversion, wxGBPosition (r, 0), wxGBSpan (1, 2), wxALIGN_CENTER_VERTICAL);
	_colour_conversion = new wxStaticText (this, wxID_ANY, wxT (""), wxDefaultPosition, size);
	grid->Add (_colour_conversion, wxGBPosition (r, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_colour_conversion_button = new wxButton (this, wxID_ANY, _("Edit..."));
	grid->Add (_colour_conversion_button, wxGBPosition (r, 3), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_description = new wxStaticText (this, wxID_ANY, wxT ("\n \n \n \n \n"), wxDefaultPosition, wxDefaultSize);
	grid->Add (_description, wxGBPosition (r, 0), wxGBSpan (1, 4), wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	wxFont font = _description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_description->SetFont(font);
	++r;

	_left_crop->wrapped()->SetRange (0, 1024);
	_top_crop->wrapped()->SetRange (0, 1024);
	_right_crop->wrapped()->SetRange (0, 1024);
	_bottom_crop->wrapped()->SetRange (0, 1024);

	vector<VideoContentScale> scales = VideoContentScale::all ();
	_scale->wrapped()->Clear ();
	for (vector<VideoContentScale>::iterator i = scales.begin(); i != scales.end(); ++i) {
		_scale->wrapped()->Append (std_to_wx (i->name ()));
	}

	_frame_type->wrapped()->Append (_("2D"));
	_frame_type->wrapped()->Append (_("3D left/right"));
	_frame_type->wrapped()->Append (_("3D top/bottom"));
	_frame_type->wrapped()->Append (_("3D alternate"));
	_frame_type->wrapped()->Append (_("3D left only"));
	_frame_type->wrapped()->Append (_("3D right only"));

	_fade_in->Changed.connect (boost::bind (&VideoPanel::fade_in_changed, this));
	_fade_out->Changed.connect (boost::bind (&VideoPanel::fade_out_changed, this));
	
	_filters_button->Bind           (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_filters_clicked, this));
	_enable_colour_conversion->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&VideoPanel::enable_colour_conversion_clicked, this));
	_colour_conversion_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_colour_conversion_clicked, this));
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
	default:
		break;
	}
}

void
VideoPanel::film_content_changed (int property)
{
	VideoContentList vc = _parent->selected_video ();
	shared_ptr<VideoContent> vcs;
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
		if (!vcs) {
			checked_set (_colour_conversion, wxT (""));
		} else if (vcs->colour_conversion ()) {
			optional<size_t> preset = vcs->colour_conversion().get().preset ();
			vector<PresetColourConversion> cc = Config::instance()->colour_conversions ();
			if (preset) {
				checked_set (_colour_conversion, std_to_wx (cc[preset.get()].name));
			} else {
				checked_set (_colour_conversion, _("Custom"));
			}
			_enable_colour_conversion->SetValue (true);
			_colour_conversion->Enable (true);
			_colour_conversion_button->Enable (true);
		} else {
			checked_set (_colour_conversion, _("None"));
			_enable_colour_conversion->SetValue (false);
			_colour_conversion->Enable (false);
			_colour_conversion_button->Enable (false);
		}
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
		set<ContentTime> check;
		for (VideoContentList::const_iterator i = vc.begin (); i != vc.end(); ++i) {
			check.insert ((*i)->fade_in ());
		}
		
		if (check.size() == 1) {
			_fade_in->set (vc.front()->fade_in (), vc.front()->video_frame_rate ());
		} else {
			_fade_in->clear ();
		}
	} else if (property == VideoContentProperty::VIDEO_FADE_OUT) {
		set<ContentTime> check;
		for (VideoContentList::const_iterator i = vc.begin (); i != vc.end(); ++i) {
			check.insert ((*i)->fade_out ());
		}
		
		if (check.size() == 1) {
			_fade_out->set (vc.front()->fade_out (), vc.front()->video_frame_rate ());
		} else {
			_fade_out->clear ();
		}
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
	VideoContentList vc = _parent->selected_video ();
	if (vc.empty ()) {
		checked_set (_description, wxT (""));
		return;
	} else if (vc.size() > 1) {
		checked_set (_description, _("Multiple content selected"));
		return;
	}

	string d = vc.front()->processing_description ();
	size_t lines = count (d.begin(), d.end(), '\n');

	for (int i = lines; i < 6; ++i) {
		d += "\n ";
	}

	checked_set (_description, d);
	_sizer->Layout ();
}

void
VideoPanel::edit_colour_conversion_clicked ()
{
	VideoContentList vc = _parent->selected_video ();
	if (vc.size() != 1) {
		return;
	}

	if (!vc.front()->colour_conversion ()) {
		return;
	}

	ColourConversion conversion = vc.front()->colour_conversion().get ();
	ContentColourConversionDialog* d = new ContentColourConversionDialog (this);
	d->set (conversion);
	d->ShowModal ();

	vc.front()->set_colour_conversion (d->get ());
	d->Destroy ();
}

void
VideoPanel::content_selection_changed ()
{
	VideoContentList video_sel = _parent->selected_video ();
	FFmpegContentList ffmpeg_sel = _parent->selected_ffmpeg ();
	
	bool const single = video_sel.size() == 1;

	_left_crop->set_content (video_sel);
	_right_crop->set_content (video_sel);
	_top_crop->set_content (video_sel);
	_bottom_crop->set_content (video_sel);
	_frame_type->set_content (video_sel);
	_scale->set_content (video_sel);

	_filters_button->Enable (single && !ffmpeg_sel.empty ());
	_colour_conversion_button->Enable (single);

	film_content_changed (VideoContentProperty::VIDEO_CROP);
	film_content_changed (VideoContentProperty::VIDEO_FRAME_RATE);
	film_content_changed (VideoContentProperty::COLOUR_CONVERSION);
	film_content_changed (VideoContentProperty::VIDEO_FADE_IN);
	film_content_changed (VideoContentProperty::VIDEO_FADE_OUT);
	film_content_changed (FFmpegContentProperty::FILTERS);
}

void
VideoPanel::fade_in_changed ()
{
	VideoContentList vc = _parent->selected_video ();
	for (VideoContentList::const_iterator i = vc.begin(); i != vc.end(); ++i) {
		(*i)->set_fade_in (_fade_in->get (_parent->film()->video_frame_rate ()));
	}
}

void
VideoPanel::fade_out_changed ()
{
	VideoContentList vc = _parent->selected_video ();
	for (VideoContentList::const_iterator i = vc.begin(); i != vc.end(); ++i) {
		(*i)->set_fade_out (_fade_out->get (_parent->film()->video_frame_rate ()));
	}
}

void
VideoPanel::enable_colour_conversion_clicked ()
{
	VideoContentList vc = _parent->selected_video ();
	if (vc.size() != 1) {
		return;
	}

	if (_enable_colour_conversion->GetValue()) {
		vc.front()->set_default_colour_conversion ();
	} else {
		vc.front()->unset_colour_conversion ();
	}
}
