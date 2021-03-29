/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_advanced_dialog.h"
#include "dcpomatic_button.h"
#include "filter_dialog.h"
#include "language_tag_widget.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/content.h"
#include "lib/dcp_content.h"
#include "lib/filter.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "lib/video_content.h"
#include <wx/gbsizer.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
#include <wx/propgrid/property.h>
#include <wx/propgrid/props.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


using std::string;
using std::vector;
using boost::bind;
using std::dynamic_pointer_cast;
using boost::optional;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;



ContentAdvancedDialog::ContentAdvancedDialog (wxWindow* parent, shared_ptr<Content> content)
	: wxDialog (parent, wxID_ANY, _("Advanced content settings"))
	, _content (content)
{
	auto sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	int r = 0;

	wxClientDC dc (this);
	auto size = dc.GetTextExtent (wxT ("A quite long name"));
#ifdef __WXGTK3__
	size.SetWidth (size.GetWidth() + 64);
#endif
	size.SetHeight (-1);

	add_label_to_sizer (sizer, this, _("Video filters"), true, wxGBPosition(r, 0));
	_filters = new StaticText (this, _("None"), wxDefaultPosition, size);
	_filters_button = new Button (this, _("Edit..."));
	auto filters = new wxBoxSizer (wxHORIZONTAL);
	filters->Add (_filters, 1, wxALL | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	filters->Add (_filters_button, 0, wxALL, DCPOMATIC_SIZER_GAP);
	sizer->Add (filters, wxGBPosition(r, 1), wxGBSpan(1, 2));
	++r;

	wxStaticText* video_frame_rate_label;
	if (_content->video) {
		video_frame_rate_label = add_label_to_sizer (sizer, this, _("Override detected video frame rate"), true, wxGBPosition(r, 0));
	} else {
		video_frame_rate_label = add_label_to_sizer (sizer, this, _("Video frame rate that content was prepared for"), true, wxGBPosition(r, 0));
	}
	_video_frame_rate = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxNumericPropertyValidator(wxNumericPropertyValidator::Float));
	sizer->Add (_video_frame_rate, wxGBPosition(r, 1));
	_set_video_frame_rate = new Button (this, _("Set"));
	_set_video_frame_rate->Enable (false);
	sizer->Add (_set_video_frame_rate, wxGBPosition(r, 2));
	++r;

	/// TRANSLATORS: next to this control is a language selector, so together they will read, for example
	/// "Video has burnt-in subtitles in the language fr-FR"
	_burnt_subtitle = new wxCheckBox (this, wxID_ANY, _("Video has burnt-in subtitles in the language"));
	sizer->Add (_burnt_subtitle, wxGBPosition(r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_burnt_subtitle_language = new LanguageTagWidget (this, _("Language of burnt-in subtitles in this content"), content->video ? content->video->burnt_subtitle_language() : boost::none);
	sizer->Add (_burnt_subtitle_language->sizer(), wxGBPosition(r, 1), wxGBSpan(1, 2), wxEXPAND);
	++r;

	auto ignore_video = new wxCheckBox (this, wxID_ANY, _("Ignore this content's video and use only audio, subtitles and closed captions"));
	sizer->Add (ignore_video, wxGBPosition(r, 0), wxGBSpan(1, 3));
	++r;

	auto overall = new wxBoxSizer (wxVERTICAL);
	overall->Add (sizer, 1, wxALL, DCPOMATIC_DIALOG_BORDER);
	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall);

	ignore_video->Enable (static_cast<bool>(_content->video));
	ignore_video->SetValue (_content->video ? !content->video->use() : false);
	setup_filters ();

	bool const single_frame_image_content = dynamic_pointer_cast<const ImageContent>(_content) && _content->number_of_paths() == 1;
	video_frame_rate_label->Enable (!single_frame_image_content);
	_video_frame_rate->Enable (!single_frame_image_content);

	auto vfr = _content->video_frame_rate ();
	if (vfr) {
		_video_frame_rate->SetValue (std_to_wx(locale_convert<string>(*vfr)));
	}

	_burnt_subtitle->SetValue (_content->video && static_cast<bool>(_content->video->burnt_subtitle_language()));
	_burnt_subtitle_language->set (_content->video ? _content->video->burnt_subtitle_language() : boost::none);

	ignore_video->Bind (wxEVT_CHECKBOX, bind(&ContentAdvancedDialog::ignore_video_changed, this, _1));
	_filters_button->Bind (wxEVT_BUTTON, bind(&ContentAdvancedDialog::edit_filters, this));
	_set_video_frame_rate->Bind (wxEVT_BUTTON, bind(&ContentAdvancedDialog::set_video_frame_rate, this));
	_video_frame_rate->Bind (wxEVT_TEXT, boost::bind(&ContentAdvancedDialog::video_frame_rate_changed, this));
	_burnt_subtitle->Bind (wxEVT_CHECKBOX, boost::bind(&ContentAdvancedDialog::burnt_subtitle_changed, this));
	_burnt_subtitle_language->Changed.connect (boost::bind(&ContentAdvancedDialog::burnt_subtitle_language_changed, this));

	setup_sensitivity ();
}


void
ContentAdvancedDialog::ignore_video_changed (wxCommandEvent& ev)
{
	 if (_content->video) {
		 _content->video->set_use (!ev.IsChecked());
	 }
}


void
ContentAdvancedDialog::setup_filters ()
{
	auto fcs = dynamic_pointer_cast<FFmpegContent>(_content);
	if (!fcs) {
		checked_set (_filters, _("None"));
		_filters->Enable (false);
		_filters_button->Enable (false);
		return;
	}

	auto p = Filter::ffmpeg_string (fcs->filters());
	if (p.empty()) {
		checked_set (_filters, _("None"));
	} else {
		if (p.length() > 25) {
			p = p.substr(0, 25) + "...";
		}
		checked_set (_filters, p);
	}
}


void
ContentAdvancedDialog::edit_filters ()
{
	auto fcs = dynamic_pointer_cast<FFmpegContent>(_content);
	if (!fcs) {
		return;
	}

	auto d = new FilterDialog (this, fcs->filters());
	d->ActiveChanged.connect (bind(&ContentAdvancedDialog::filters_changed, this, _1));
	d->ShowModal ();
	d->Destroy ();
}


void
ContentAdvancedDialog::filters_changed (vector<Filter const *> filters)
{
	auto fcs = dynamic_pointer_cast<FFmpegContent>(_content);
	if (!fcs) {
		return;
	}

	fcs->set_filters (filters);
	setup_filters ();
}


void
ContentAdvancedDialog::set_video_frame_rate ()
{
	if (_video_frame_rate->GetValue() != wxT("")) {
		_content->set_video_frame_rate (locale_convert<double>(wx_to_std(_video_frame_rate->GetValue())));
	} else {
		_content->unset_video_frame_rate ();
	}

	_set_video_frame_rate->Enable (false);
}


void
ContentAdvancedDialog::video_frame_rate_changed ()
{
       bool enable = true;
       /* If the user clicks "set" now, with no frame rate entered, it would unset the video
	  frame rate in the selected content.  This can't be allowed for some content types.
       */
       if (_video_frame_rate->GetValue() == wxT("") && (dynamic_pointer_cast<DCPContent>(_content) || dynamic_pointer_cast<FFmpegContent>(_content))) {
	       enable = false;
       }

       _set_video_frame_rate->Enable (enable);
}


void
ContentAdvancedDialog::setup_sensitivity ()
{
	_burnt_subtitle->Enable (static_cast<bool>(_content->video));
	_burnt_subtitle_language->enable (_content->video && _burnt_subtitle->GetValue());
}


void
ContentAdvancedDialog::burnt_subtitle_changed ()
{
	setup_sensitivity ();
}


void
ContentAdvancedDialog::burnt_subtitle_language_changed ()
{
	DCPOMATIC_ASSERT (_content->video);
	_content->video->set_burnt_subtitle_language (_burnt_subtitle_language->get());
}

