/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/spinctrl.h>
#include "lib/ratio.h"
#include "lib/filter.h"
#include "filter_dialog.h"
#include "video_panel.h"
#include "wx_util.h"
#include "film_editor.h"

using std::vector;
using std::string;
using std::pair;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;

VideoPanel::VideoPanel (FilmEditor* e)
	: FilmEditorPanel (e, _("Video"))
{
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;

	add_label_to_grid_bag_sizer (grid, this, _("Type"), true, wxGBPosition (r, 0));
	_frame_type = new wxChoice (this, wxID_ANY);
	grid->Add (_frame_type, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Left crop"), true, wxGBPosition (r, 0));
	_left_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_left_crop, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Right crop"), true, wxGBPosition (r, 0));
	_right_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_right_crop, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Top crop"), true, wxGBPosition (r, 0));
	_top_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_top_crop, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Bottom crop"), true, wxGBPosition (r, 0));
	_bottom_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_bottom_crop, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Scale to"), true, wxGBPosition (r, 0));
	_ratio = new wxChoice (this, wxID_ANY);
	grid->Add (_ratio, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, this, _("Filters"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_filters = new wxStaticText (this, wxID_ANY, _("None"));
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_filters_button, 0, wxALIGN_CENTER_VERTICAL);
		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;

	_description = new wxStaticText (this, wxID_ANY, wxT ("\n \n \n \n \n"), wxDefaultPosition, wxDefaultSize);
	grid->Add (_description, wxGBPosition (r, 0), wxGBSpan (1, 2), wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	wxFont font = _description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_description->SetFont(font);
	++r;

	_left_crop->SetRange (0, 1024);
	_top_crop->SetRange (0, 1024);
	_right_crop->SetRange (0, 1024);
	_bottom_crop->SetRange (0, 1024);

	vector<Ratio const *> ratios = Ratio::all ();
	_ratio->Clear ();
	for (vector<Ratio const *>::iterator i = ratios.begin(); i != ratios.end(); ++i) {
		_ratio->Append (std_to_wx ((*i)->nickname ()));
	}

	_frame_type->Append (_("2D"));
	_frame_type->Append (_("3D left/right"));

	_frame_type->Bind     (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&VideoPanel::frame_type_changed, this));
	_left_crop->Bind      (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::left_crop_changed, this));
	_right_crop->Bind     (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::right_crop_changed, this));
	_top_crop->Bind       (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::top_crop_changed, this));
	_bottom_crop->Bind    (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::bottom_crop_changed, this));
	_ratio->Bind	      (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&VideoPanel::ratio_changed, this));
	_filters_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_filters_clicked, this));
}


/** Called when the left crop widget has been changed */
void
VideoPanel::left_crop_changed ()
{
	shared_ptr<VideoContent> c = _editor->selected_video_content ();
	if (!c) {
		return;
	}

	c->set_left_crop (_left_crop->GetValue ());
}

/** Called when the right crop widget has been changed */
void
VideoPanel::right_crop_changed ()
{
	shared_ptr<VideoContent> c = _editor->selected_video_content ();
	if (!c) {
		return;
	}

	c->set_right_crop (_right_crop->GetValue ());
}

/** Called when the top crop widget has been changed */
void
VideoPanel::top_crop_changed ()
{
	shared_ptr<VideoContent> c = _editor->selected_video_content ();
	if (!c) {
		return;
	}

	c->set_top_crop (_top_crop->GetValue ());
}

/** Called when the bottom crop value has been changed */
void
VideoPanel::bottom_crop_changed ()
{
	shared_ptr<VideoContent> c = _editor->selected_video_content ();
	if (!c) {
		return;
	}

	c->set_bottom_crop (_bottom_crop->GetValue ());
}

void
VideoPanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::CONTAINER:
	case Film::VIDEO_FRAME_RATE:
		setup_description ();
		break;
	default:
		break;
	}
}

void
VideoPanel::film_content_changed (shared_ptr<Content> c, int property)
{
	shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (c);
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);

	if (property == VideoContentProperty::VIDEO_FRAME_TYPE) {
		checked_set (_frame_type, vc ? vc->video_frame_type () : VIDEO_FRAME_TYPE_2D);
	} else if (property == VideoContentProperty::VIDEO_CROP) {
		checked_set (_left_crop,   vc ? vc->crop().left	: 0);
		checked_set (_right_crop,  vc ? vc->crop().right	: 0);
		checked_set (_top_crop,	   vc ? vc->crop().top	: 0);
		checked_set (_bottom_crop, vc ? vc->crop().bottom : 0);
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_RATIO) {
		if (vc) {
			int n = 0;
			vector<Ratio const *> ratios = Ratio::all ();
			vector<Ratio const *>::iterator i = ratios.begin ();
			while (i != ratios.end() && *i != vc->ratio()) {
				++i;
				++n;
			}

			if (i == ratios.end()) {
				checked_set (_ratio, -1);
			} else {
				checked_set (_ratio, n);
			}
		} else {
			checked_set (_ratio, -1);
		}
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_FRAME_RATE) {
		setup_description ();
	} else if (property == FFmpegContentProperty::FILTERS) {
		if (fc) {
			pair<string, string> p = Filter::ffmpeg_strings (fc->filters ());
			if (p.first.empty () && p.second.empty ()) {
				_filters->SetLabel (_("None"));
			} else {
				string const b = p.first + " " + p.second;
				_filters->SetLabel (std_to_wx (b));
			}
		}
	}
}

/** Called when the `Edit filters' button has been clicked */
void
VideoPanel::edit_filters_clicked ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}
	
	FilterDialog* d = new FilterDialog (this, fc->filters());
	d->ActiveChanged.connect (bind (&FFmpegContent::set_filters, fc, _1));
	d->ShowModal ();
	d->Destroy ();
}

void
VideoPanel::setup_description ()
{
	shared_ptr<VideoContent> vc = _editor->selected_video_content ();
	if (!vc) {
		_description->SetLabel ("");
		return;
	}

	wxString d;

	int lines = 0;

	if (vc->video_size().width && vc->video_size().height) {
		d << wxString::Format (
			_("Content video is %dx%d (%.2f:1)\n"),
			vc->video_size().width, vc->video_size().height,
			float (vc->video_size().width) / vc->video_size().height
			);
		++lines;
	}

	Crop const crop = vc->crop ();
	if ((crop.left || crop.right || crop.top || crop.bottom) && vc->video_size() != libdcp::Size (0, 0)) {
		libdcp::Size cropped = vc->video_size ();
		cropped.width -= crop.left + crop.right;
		cropped.height -= crop.top + crop.bottom;
		d << wxString::Format (
			_("Cropped to %dx%d (%.2f:1)\n"),
			cropped.width, cropped.height,
			float (cropped.width) / cropped.height
			);
		++lines;
	}

	Ratio const * ratio = vc->ratio ();
	if (ratio) {
		libdcp::Size container_size = _editor->film()->container()->size (_editor->film()->full_frame ());
		
		libdcp::Size const scaled = ratio->size (container_size);
		d << wxString::Format (
			_("Scaled to %dx%d (%.2f:1)\n"),
			scaled.width, scaled.height,
			float (scaled.width) / scaled.height
			);
		++lines;

		if (scaled != container_size) {
			d << wxString::Format (
				_("Padded with black to %dx%d (%.2f:1)\n"),
				container_size.width, container_size.height,
				float (container_size.width) / container_size.height
				);
			++lines;
		}
	}

	d << wxString::Format (_("Content frame rate %.4f\n"), vc->video_frame_rate ());
	++lines;
	FrameRateConversion frc (vc->video_frame_rate(), _editor->film()->video_frame_rate ());
	d << frc.description << "\n";
	++lines;

	for (int i = lines; i < 6; ++i) {
		d << wxT ("\n ");
	}

	_description->SetLabel (d);
	_sizer->Layout ();
}


void
VideoPanel::ratio_changed ()
{
	if (!_editor->film ()) {
		return;
	}

	shared_ptr<VideoContent> vc = _editor->selected_video_content ();
	
	int const n = _ratio->GetSelection ();
	if (n >= 0) {
		vector<Ratio const *> ratios = Ratio::all ();
		assert (n < int (ratios.size()));
		vc->set_ratio (ratios[n]);
	}
}

void
VideoPanel::frame_type_changed ()
{
	shared_ptr<VideoContent> vc = _editor->selected_video_content ();
	if (vc) {
		vc->set_video_frame_type (static_cast<VideoFrameType> (_frame_type->GetSelection ()));
	}
}
