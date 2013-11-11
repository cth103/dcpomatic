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
#include "lib/ffmpeg_content.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/util.h"
#include "filter_dialog.h"
#include "video_panel.h"
#include "wx_util.h"
#include "film_editor.h"
#include "content_colour_conversion_dialog.h"
#include "multiple_widget.h"

using std::vector;
using std::string;
using std::pair;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using boost::optional;

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
	_left_crop = new MultipleWidget<wxSpinCtrl> (this, new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)));
	_left_crop->add (grid, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Right crop"), true, wxGBPosition (r, 0));
	_right_crop = new MultipleWidget<wxSpinCtrl> (this, new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)));
	_right_crop->add (grid, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Top crop"), true, wxGBPosition (r, 0));
	_top_crop = new MultipleWidget<wxSpinCtrl> (this, new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)));
	_top_crop->add (grid, wxGBPosition (r,1 ));
	++r;
	
	add_label_to_grid_bag_sizer (grid, this, _("Bottom crop"), true, wxGBPosition (r, 0));
	_bottom_crop = new MultipleWidget<wxSpinCtrl> (this, new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1)));
	_bottom_crop->add (grid, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Scale to"), true, wxGBPosition (r, 0));
	_ratio = new wxChoice (this, wxID_ANY);
	grid->Add (_ratio, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, this, _("Filters"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		wxClientDC dc (this);
		wxSize size = dc.GetTextExtent (wxT ("A quite long name"));
		size.SetHeight (-1);
		
		_filters = new wxStaticText (this, wxID_ANY, _("None"), wxDefaultPosition, size);
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_filters_button, 0, wxALIGN_CENTER_VERTICAL);
		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;
	
	{
		add_label_to_grid_bag_sizer (grid, this, _("Colour conversion"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		wxClientDC dc (this);
		wxSize size = dc.GetTextExtent (wxT ("A quite long name"));
		size.SetHeight (-1);
		
		_colour_conversion = new wxStaticText (this, wxID_ANY, wxT (""), wxDefaultPosition, size);

		s->Add (_colour_conversion, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_colour_conversion_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_colour_conversion_button, 0, wxALIGN_CENTER_VERTICAL);
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

	_left_crop->wrapped()->SetRange (0, 1024);
	_top_crop->wrapped()->SetRange (0, 1024);
	_right_crop->wrapped()->SetRange (0, 1024);
	_bottom_crop->wrapped()->SetRange (0, 1024);

	vector<Ratio const *> ratios = Ratio::all ();
	_ratio->Clear ();
	for (vector<Ratio const *>::iterator i = ratios.begin(); i != ratios.end(); ++i) {
		_ratio->Append (std_to_wx ((*i)->nickname ()));
	}
	_ratio->Append (_("No stretch"));

	_frame_type->Append (_("2D"));
	_frame_type->Append (_("3D left/right"));

	_frame_type->Bind               (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&VideoPanel::frame_type_changed, this));
	_left_crop->SetAllSame.connect  (boost::bind (&VideoPanel::set_left_crop_same, this));
	_left_crop->wrapped()->Bind     (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::left_crop_changed, this));
	_right_crop->wrapped()->Bind    (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::right_crop_changed, this));
	_top_crop->wrapped()->Bind      (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::top_crop_changed, this));
	_bottom_crop->wrapped()->Bind   (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&VideoPanel::bottom_crop_changed, this));
	_ratio->Bind	                (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&VideoPanel::ratio_changed, this));
	_filters_button->Bind           (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_filters_clicked, this));
	_colour_conversion_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&VideoPanel::edit_colour_conversion_clicked, this));
}


/** Called when the left crop widget has been changed */
void
VideoPanel::left_crop_changed ()
{
	VideoContentList c = _editor->selected_video_content ();
	for (VideoContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_left_crop (_left_crop->wrapped()->GetValue ());
	}
}

/** Called when the right crop widget has been changed */
void
VideoPanel::right_crop_changed ()
{
	VideoContentList c = _editor->selected_video_content ();
	for (VideoContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_right_crop (_right_crop->wrapped()->GetValue ());
	}
}

/** Called when the top crop widget has been changed */
void
VideoPanel::top_crop_changed ()
{
	VideoContentList c = _editor->selected_video_content ();
	for (VideoContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_top_crop (_top_crop->wrapped()->GetValue ());
	}
}

/** Called when the bottom crop value has been changed */
void
VideoPanel::bottom_crop_changed ()
{
	VideoContentList c = _editor->selected_video_content ();
	for (VideoContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_bottom_crop (_bottom_crop->wrapped()->GetValue ());
	}
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
VideoPanel::film_content_changed (int property)
{
	VideoContentList vc = _editor->selected_video_content ();
	shared_ptr<VideoContent> vcs;
	shared_ptr<FFmpegContent> fcs;
	if (!vc.empty ()) {
		vcs = vc.front ();
		fcs = dynamic_pointer_cast<FFmpegContent> (vcs);
	}
	
	if (property == VideoContentProperty::VIDEO_FRAME_TYPE) {
		checked_set (_frame_type, vcs ? vcs->video_frame_type () : VIDEO_FRAME_TYPE_2D);
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_CROP) {
		set_multiple<VideoContent> (vc, _left_crop, &VideoContent::left_crop);
		set_multiple<VideoContent> (vc, _right_crop, &VideoContent::right_crop);
		set_multiple<VideoContent> (vc, _top_crop, &VideoContent::top_crop);
		set_multiple<VideoContent> (vc, _bottom_crop, &VideoContent::bottom_crop);
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_RATIO) {
		if (vcs) {
			int n = 0;
			vector<Ratio const *> ratios = Ratio::all ();
			vector<Ratio const *>::iterator i = ratios.begin ();
			while (i != ratios.end() && *i != vcs->ratio()) {
				++i;
				++n;
			}

			if (i == ratios.end()) {
				checked_set (_ratio, ratios.size ());
			} else {
				checked_set (_ratio, n);
			}
		} else {
			checked_set (_ratio, -1);
		}
		setup_description ();
	} else if (property == VideoContentProperty::VIDEO_FRAME_RATE) {
		setup_description ();
	} else if (property == VideoContentProperty::COLOUR_CONVERSION) {
		optional<size_t> preset = vcs ? vcs->colour_conversion().preset () : optional<size_t> ();
		vector<PresetColourConversion> cc = Config::instance()->colour_conversions ();
		_colour_conversion->SetLabel (preset ? std_to_wx (cc[preset.get()].name) : _("Custom"));
	} else if (property == FFmpegContentProperty::FILTERS) {
		if (fcs) {
			pair<string, string> p = Filter::ffmpeg_strings (fcs->filters ());
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
	FFmpegContentList c = _editor->selected_ffmpeg_content ();
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
	FFmpegContentList vc = _editor->selected_ffmpeg_content ();
	if (vc.empty ()) {
		_description->SetLabel ("");
		return;
	} else if (vc.size() > 1) {
		_description->SetLabel (_("Multiple content selected"));
		return;
	}

	shared_ptr<FFmpegContent> vcs = vc.front ();

	wxString d;

	int lines = 0;

	if (vcs->video_size().width && vcs->video_size().height) {
		d << wxString::Format (
			_("Content video is %dx%d (%.2f:1)\n"),
			vcs->video_size_after_3d_split().width,
			vcs->video_size_after_3d_split().height,
			vcs->video_size_after_3d_split().ratio ()
			);
		++lines;
	}

	Crop const crop = vcs->crop ();
	if ((crop.left || crop.right || crop.top || crop.bottom) && vcs->video_size() != libdcp::Size (0, 0)) {
		libdcp::Size cropped = vcs->video_size_after_crop ();
		d << wxString::Format (
			_("Cropped to %dx%d (%.2f:1)\n"),
			cropped.width, cropped.height,
			cropped.ratio ()
			);
		++lines;
	}

	Ratio const * ratio = vcs->ratio ();
	libdcp::Size container_size = fit_ratio_within (_editor->film()->container()->ratio (), _editor->film()->full_frame ());
	float const ratio_value = ratio ? ratio->ratio() : vcs->video_size_after_crop().ratio ();

	/* We have a specified ratio to scale to */
	libdcp::Size const scaled = fit_ratio_within (ratio_value, container_size);
	
	d << wxString::Format (
		_("Scaled to %dx%d (%.2f:1)\n"),
		scaled.width, scaled.height,
		scaled.ratio ()
		);
	++lines;
	
	if (scaled != container_size) {
		d << wxString::Format (
			_("Padded with black to %dx%d (%.2f:1)\n"),
			container_size.width, container_size.height,
			container_size.ratio ()
			);
		++lines;
	}

	d << wxString::Format (_("Content frame rate %.4f\n"), vcs->video_frame_rate ());
	++lines;
	FrameRateConversion frc (vcs->video_frame_rate(), _editor->film()->video_frame_rate ());
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

	VideoContentList vc = _editor->selected_video_content ();
	if (vc.size() != 1) {
		return;
	}
	
	int const n = _ratio->GetSelection ();
	if (n >= 0) {
		vector<Ratio const *> ratios = Ratio::all ();
		if (n < int (ratios.size ())) {
			vc.front()->set_ratio (ratios[n]);
		} else {
			vc.front()->set_ratio (0);
		}
	}
}

void
VideoPanel::frame_type_changed ()
{
	VideoContentList vc = _editor->selected_video_content ();
	if (vc.size() == 1) {
		vc.front()->set_video_frame_type (static_cast<VideoFrameType> (_frame_type->GetSelection ()));
	}
}

void
VideoPanel::edit_colour_conversion_clicked ()
{
	VideoContentList vc = _editor->selected_video_content ();
	if (vc.size() != 1) {
		return;
	}

	ColourConversion conversion = vc.front()->colour_conversion ();
	ContentColourConversionDialog* d = new ContentColourConversionDialog (this);
	d->set (conversion);
	d->ShowModal ();

	vc.front()->set_colour_conversion (d->get ());
	d->Destroy ();
}

void
VideoPanel::content_selection_changed ()
{
	VideoContentList sel = _editor->selected_video_content ();
	bool const single = sel.size() == 1;
	bool const multiple = sel.size() > 1;

	/* Things that are allowed with multiple selections */
	_left_crop->wrapped()->Enable   (single || multiple);
	_right_crop->wrapped()->Enable  (single || multiple);
	_top_crop->wrapped()->Enable    (single || multiple);
	_bottom_crop->wrapped()->Enable (single || multiple);

	/* Things that are only allowed with single selections */
	_frame_type->Enable (single);
	_ratio->Enable (single);
	_filters_button->Enable (single);
	_colour_conversion_button->Enable (single);

	film_content_changed (VideoContentProperty::VIDEO_FRAME_TYPE);
	film_content_changed (VideoContentProperty::VIDEO_CROP);
	film_content_changed (VideoContentProperty::VIDEO_RATIO);
	film_content_changed (VideoContentProperty::VIDEO_FRAME_RATE);
	film_content_changed (VideoContentProperty::COLOUR_CONVERSION);
	film_content_changed (FFmpegContentProperty::FILTERS);
}

void
VideoPanel::set_left_crop_same ()
{
	VideoContentList sel = _editor->selected_video_content ();
	for (VideoContentList::iterator i = sel.begin(); i != sel.end(); ++i) {
		(*i)->set_left_crop (sel.front()->left_crop ());
	}
}

