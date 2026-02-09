/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
#include "content_colour_conversion_dialog.h"
#include "content_panel.h"
#include "content_widget.h"
#include "custom_scale_dialog.h"
#include "dcpomatic_button.h"
#include "filter_dialog.h"
#include "static_text.h"
#include "video_panel.h"
#include "wx_util.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/frame_rate_change.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <set>
#include <iostream>


using std::vector;
using std::string;
using std::pair;
using std::cout;
using std::list;
using std::set;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using boost::bind;
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


VideoPanel::VideoPanel(ContentPanel* p)
	: ContentSubPanel(p, _("Video"))
{

}


void
VideoPanel::create()
{
	_type_label = create_label(this, _("Type"), true);
	_frame_type = new ContentChoice<VideoContent, VideoFrameType>(
		this,
		new wxChoice(this, wxID_ANY),
		VideoContentProperty::FRAME_TYPE,
		&Content::video,
		boost::mem_fn(&VideoContent::frame_type),
		boost::mem_fn(&VideoContent::set_frame_type),
		&caster<int, VideoFrameType>,
		&caster<VideoFrameType, int>
		);

	_crop_label = create_label(this, _("Crop"), true);

	_left_crop_label = create_label(this, _("Left"), true);
	_left_crop = new ContentSpinCtrl<VideoContent>(
		this,
		new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(dcpomatic::wx::linked_value_width(), -1)),
		VideoContentProperty::CROP,
		&Content::video,
		boost::mem_fn(&VideoContent::requested_left_crop),
		boost::mem_fn(&VideoContent::set_left_crop),
		boost::bind(&VideoPanel::left_crop_changed, this)
		);

	_left_right_link = new wxToggleButton(this, wxID_ANY, {}, wxDefaultPosition, dcpomatic::wx::link_size(this));
	_left_right_link->SetBitmap(wxBitmap(dcpomatic::wx::link_bitmap_path(), wxBITMAP_TYPE_PNG));

	_right_crop_label = create_label(this, _("Right"), true);
	_right_crop = new ContentSpinCtrl<VideoContent>(
		this,
		new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(dcpomatic::wx::linked_value_width(), -1)),
		VideoContentProperty::CROP,
		&Content::video,
		boost::mem_fn(&VideoContent::requested_right_crop),
		boost::mem_fn(&VideoContent::set_right_crop),
		boost::bind(&VideoPanel::right_crop_changed, this)
		);

	_top_crop_label = create_label(this, _("Top"), true);
	_top_crop = new ContentSpinCtrl<VideoContent>(
		this,
		new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(dcpomatic::wx::linked_value_width(), -1)),
		VideoContentProperty::CROP,
		&Content::video,
		boost::mem_fn(&VideoContent::requested_top_crop),
		boost::mem_fn(&VideoContent::set_top_crop),
		boost::bind(&VideoPanel::top_crop_changed, this)
		);

	_top_bottom_link = new wxToggleButton(this, wxID_ANY, {}, wxDefaultPosition, dcpomatic::wx::link_size(this));
	_top_bottom_link->SetBitmap(wxBitmap(dcpomatic::wx::link_bitmap_path(), wxBITMAP_TYPE_PNG));

	_bottom_crop_label = create_label(this, _("Bottom"), true);
	_bottom_crop = new ContentSpinCtrl<VideoContent>(
		this,
		new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(dcpomatic::wx::linked_value_width(), -1)),
		VideoContentProperty::CROP,
		&Content::video,
		boost::mem_fn(&VideoContent::requested_bottom_crop),
		boost::mem_fn(&VideoContent::set_bottom_crop),
		boost::bind(&VideoPanel::bottom_crop_changed, this)
		);

	_fade_in_label = create_label(this, _("Fade in"), true);
	_fade_in = new Timecode<ContentTime>(this);

	_fade_out_label = create_label(this, _("Fade out"), true);
	_fade_out = new Timecode<ContentTime>(this);

	wxClientDC dc(this);
	auto size = dc.GetTextExtent(char_to_wx("A quite long name"));
#ifdef __WXGTK3__
	size.SetWidth(size.GetWidth() + 64);
#endif
	size.SetHeight(-1);

	_scale_label = create_label(this, _("Scale"), true);
	_scale_fit = new wxRadioButton(this, wxID_ANY, _("to fit DCP"));
	_scale_custom = new wxRadioButton(this, wxID_ANY, _("custom"));
	_scale_custom_edit = new Button(this, _("Edit..."), wxDefaultPosition, small_button_size(this, _("Edit...")));

	_colour_conversion_label = create_label(this, _("Source\ncolourspace"), true);
	_colour_conversion = new wxChoice(this, wxID_ANY, wxDefaultPosition, size);
	_colour_conversion->Append(_("DCI X'Y'Z'"));
	for (auto const& i: PresetColourConversion::all()) {
		_colour_conversion->Append(std_to_wx(i.name));
	}

	/// TRANSLATORS: translate the word "Custom" here; do not include the "Colour|" prefix
	_colour_conversion->Append(S_("Colour|Custom"));
	_edit_colour_conversion_button = new Button(this, _("Edit..."), wxDefaultPosition, small_button_size(this, _("Edit...")));

	_range_label = create_label(this, _("Range"), true);
	_range = new wxChoice(this, wxID_ANY);
	_range->Append(_("Full (JPEG, 0-255)"));
	_range->Append(_("Video (MPEG, 16-235)"));

	_description = new StaticText(this, char_to_wx("\n \n \n \n \n"), wxDefaultPosition, wxDefaultSize);
	auto font = _description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_description->SetFont(font);

	_left_crop->wrapped()->SetRange(0, 4096);
	_top_crop->wrapped()->SetRange(0, 4096);
	_right_crop->wrapped()->SetRange(0, 4096);
	_bottom_crop->wrapped()->SetRange(0, 4096);

	_frame_type->wrapped()->Append(_("2D"));
	_frame_type->wrapped()->Append(_("3D"));
	_frame_type->wrapped()->Append(_("3D left/right"));
	_frame_type->wrapped()->Append(_("3D top/bottom"));
	_frame_type->wrapped()->Append(_("3D alternate"));
	_frame_type->wrapped()->Append(_("3D left only"));
	_frame_type->wrapped()->Append(_("3D right only"));

	content_selection_changed();

	_fade_in->Changed.connect(boost::bind(&VideoPanel::fade_in_changed, this));
	_fade_out->Changed.connect(boost::bind(&VideoPanel::fade_out_changed, this));

	_scale_fit->Bind                    (wxEVT_RADIOBUTTON, boost::bind(&VideoPanel::scale_fit_clicked, this));
	_scale_custom->Bind                 (wxEVT_RADIOBUTTON, boost::bind(&VideoPanel::scale_custom_clicked, this));
	_scale_custom_edit->Bind            (wxEVT_BUTTON,   boost::bind(&VideoPanel::scale_custom_edit_clicked, this));
	_colour_conversion->Bind            (wxEVT_CHOICE,   boost::bind(&VideoPanel::colour_conversion_changed, this));
	_range->Bind                        (wxEVT_CHOICE,   boost::bind(&VideoPanel::range_changed, this));
	_edit_colour_conversion_button->Bind(wxEVT_BUTTON,   boost::bind(&VideoPanel::edit_colour_conversion_clicked, this));
	_left_right_link->Bind              (wxEVT_TOGGLEBUTTON, boost::bind(&VideoPanel::left_right_link_clicked, this));
	_top_bottom_link->Bind              (wxEVT_TOGGLEBUTTON, boost::bind(&VideoPanel::top_bottom_link_clicked, this));

	add_to_grid();

	_sizer->Layout();
}


void
VideoPanel::add_to_grid()
{
	int r = 0;

	add_label_to_sizer(_grid, _type_label, true, wxGBPosition(r, 0));
	_frame_type->add(_grid, wxGBPosition(r, 1), wxGBSpan(1, 2));
	++r;

	int cr = 0;
	auto crop = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_sizer(crop, _left_crop_label, true, wxGBPosition(cr, 0));
	_left_crop->add(crop, wxGBPosition(cr, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
#ifdef __WXGTK3__
	crop->Add(_left_right_link, wxGBPosition(cr, 2), wxGBSpan(2, 1));
	++cr;
	add_label_to_sizer(crop, _right_crop_label, true, wxGBPosition(cr, 0));
	_right_crop->add(crop, wxGBPosition(cr, 1));
#else
	crop->Add(_left_right_link, wxGBPosition(cr, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	add_label_to_sizer(crop, _right_crop_label, true, wxGBPosition(cr, 3));
	_right_crop->add(crop, wxGBPosition(cr, 4), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
#endif
	++cr;
	add_label_to_sizer(crop, _top_crop_label, true, wxGBPosition(cr, 0));
	_top_crop->add(crop, wxGBPosition(cr, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
#ifdef __WXGTK3__
	crop->Add(_top_bottom_link, wxGBPosition(cr, 2), wxGBSpan(2, 1));
	++cr;
	add_label_to_sizer(crop, _bottom_crop_label, true, wxGBPosition(cr, 0));
	_bottom_crop->add(crop, wxGBPosition(cr, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
#else
	crop->Add(_top_bottom_link, wxGBPosition(cr, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	add_label_to_sizer(crop, _bottom_crop_label, true, wxGBPosition(cr, 3));
	_bottom_crop->add(crop, wxGBPosition(cr, 4), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
#endif
	add_label_to_sizer(_grid, _crop_label, true, wxGBPosition(r, 0));
	_grid->Add(crop, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(_grid, _fade_in_label, true, wxGBPosition(r, 0));
	_grid->Add(_fade_in, wxGBPosition(r, 1), wxGBSpan(1, 3));
	++r;

	add_label_to_sizer(_grid, _fade_out_label, true, wxGBPosition(r, 0));
	_grid->Add(_fade_out, wxGBPosition(r, 1), wxGBSpan(1, 3));
	++r;

	add_label_to_sizer(_grid, _scale_label, true, wxGBPosition(r, 0));
	{
		auto v = new wxBoxSizer(wxVERTICAL);
		v->Add(_scale_fit, 0, wxBOTTOM, 4);
		auto h = new wxBoxSizer(wxHORIZONTAL);
		h->Add(_scale_custom, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 6);
		h->Add(_scale_custom_edit, 0, wxALIGN_CENTER_VERTICAL);
		v->Add(h, 0);
		_grid->Add(v, wxGBPosition(r, 1));
	}
	++r;

	add_label_to_sizer(_grid, _colour_conversion_label, true, wxGBPosition(r, 0));
	{
		auto s = new wxBoxSizer(wxHORIZONTAL);
		s->Add(_colour_conversion, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		s->Add(_edit_colour_conversion_button, 0, wxALIGN_CENTER_VERTICAL);
		_grid->Add(s, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;

	add_label_to_sizer(_grid, _range_label, true, wxGBPosition(r, 0));
	_grid->Add(_range, wxGBPosition(r, 1), wxGBSpan(1, 2), wxALIGN_CENTER_VERTICAL);
	++r;

	_grid->Add(_description, wxGBPosition(r, 0), wxGBSpan(1, 4), wxEXPAND | wxALIGN_CENTER_VERTICAL, 6);
	++r;
}


void
VideoPanel::range_changed()
{
	auto vc = _parent->selected_video();
	if (vc.size() != 1) {
		return;
	}

	switch (_range->GetSelection()) {
	case 0:
		vc.front()->video->set_range(VideoRange::FULL);
		break;
	case 1:
		vc.front()->video->set_range(VideoRange::VIDEO);
		break;
	default:
		DCPOMATIC_ASSERT(false);
	}
}


void
VideoPanel::film_changed(FilmProperty property)
{
	switch (property) {
	case FilmProperty::VIDEO_FRAME_RATE:
	case FilmProperty::CONTAINER:
	case FilmProperty::RESOLUTION:
		setup_description();
		setup_sensitivity();
		break;
	case FilmProperty::REEL_TYPE:
	case FilmProperty::INTEROP:
		setup_sensitivity();
		break;
	default:
		break;
	}
}


std::size_t
hash_value (boost::optional<ColourConversion> const & c)
{
	boost::hash<string> hasher;
	if (!c) {
		return hasher("none");
	}
	return hasher(c->identifier());
}


void
VideoPanel::film_content_changed(int property)
{
	auto vc = _parent->selected_video();
	shared_ptr<Content> vcs;
	shared_ptr<FFmpegContent> fcs;
	if (!vc.empty()) {
		vcs = vc.front();
		fcs = dynamic_pointer_cast<FFmpegContent>(vcs);
	}

	switch (property) {
	case ContentProperty::VIDEO_FRAME_RATE:
	case VideoContentProperty::FRAME_TYPE:
	case VideoContentProperty::CROP:
		setup_description();
		break;
	case VideoContentProperty::COLOUR_CONVERSION:
	{
		boost::unordered_set<optional<ColourConversion>> check;
		for (auto i: vc) {
			check.insert(i->video->colour_conversion());
		}

		/* Remove any "Many" entry that we might have added previously.  There should
		 * be entries for each preset plus one for "DCI X'Y'Z'" and one for "Custom".
		 */
		auto cc = PresetColourConversion::all();
		if (_colour_conversion->GetCount() > cc.size() + 2) {
			_colour_conversion->Delete(_colour_conversion->GetCount() - 1);
		}

		if (check.size() == 1) {
			if (vcs && vcs->video->colour_conversion()) {
				auto preset = vcs->video->colour_conversion().get().preset();
				if (preset) {
					checked_set(_colour_conversion, preset.get() + 1);
				} else {
					checked_set(_colour_conversion, cc.size() + 1);
				}
			} else {
				checked_set(_colour_conversion, 0);
			}
		} else if (check.size() > 1) {
			/* Add a "many" entry and select it as an indication that multiple different
			 * colour conversions are present in the selection.
			 */
			_colour_conversion->Append(_("Many"));
			checked_set(_colour_conversion, _colour_conversion->GetCount() - 1);
		}

		setup_sensitivity();
		break;
	}
	case VideoContentProperty::USE:
		setup_sensitivity();
		break;
	case VideoContentProperty::FADE_IN:
	{
		set<Frame> check;
		for (auto i: vc) {
			check.insert(i->video->fade_in());
		}

		if (check.size() == 1) {
			_fade_in->set(
				ContentTime::from_frames(vc.front()->video->fade_in(), vc.front()->active_video_frame_rate(_parent->film())),
				vc.front()->active_video_frame_rate(_parent->film())
				);
		} else {
			_fade_in->clear();
		}
		break;
	}
	case VideoContentProperty::FADE_OUT:
	{
		set<Frame> check;
		for (auto i: vc) {
			check.insert(i->video->fade_out());
		}

		if (check.size() == 1) {
			_fade_out->set(
				ContentTime::from_frames(vc.front()->video->fade_out(), vc.front()->active_video_frame_rate(_parent->film())),
				vc.front()->active_video_frame_rate(_parent->film())
				);
		} else {
			_fade_out->clear();
		}
		break;
	}
	case VideoContentProperty::RANGE:
		if (vcs) {
			checked_set(_range, vcs->video->range() == VideoRange::FULL ? 0 : 1);
		} else {
			checked_set(_range, 0);
		}

		setup_sensitivity();
		break;
	case VideoContentProperty::CUSTOM_RATIO:
	case VideoContentProperty::CUSTOM_SIZE:
	{
		set<Frame> check;
		for (auto i: vc) {
			check.insert(i->video->custom_ratio() || i->video->custom_size());
		}

		if (check.size() == 1) {
			checked_set(_scale_fit, !vc.front()->video->custom_ratio() && !vc.front()->video->custom_size());
			checked_set(_scale_custom, vc.front()->video->custom_ratio() || vc.front()->video->custom_size());
		} else {
			checked_set(_scale_fit, true);
			checked_set(_scale_custom, false);
		}
		setup_sensitivity();
		setup_description();
		break;
	}
	case DCPContentProperty::REFERENCE_VIDEO:
	case DCPContentProperty::REFERENCE_AUDIO:
	case DCPContentProperty::REFERENCE_TEXT:
		setup_sensitivity();
		break;
	}
}


void
VideoPanel::setup_description()
{
	auto vc = _parent->selected_video();
	if (vc.empty()) {
		checked_set(_description, wxString{});
		return;
	} else if (vc.size() > 1) {
		checked_set(_description, _("Multiple content selected"));
		return;
	}

	auto d = vc.front()->video->processing_description(_parent->film());
	size_t lines = count(d.begin(), d.end(), '\n');

	for (int i = lines; i < 6; ++i) {
		d += "\n ";
	}

	checked_set(_description, d);
	layout();
}


void
VideoPanel::colour_conversion_changed()
{
	auto vc = _parent->selected_video();

	int const s = _colour_conversion->GetSelection();
	auto all = PresetColourConversion::all();

	if (s == int(all.size() + 1)) {
		edit_colour_conversion_clicked();
	} else {
		for (auto i: _parent->selected_video()) {
			if (s == 0) {
				i->video->unset_colour_conversion();
			} else if (s != int(all.size() + 2)) {
				i->video->set_colour_conversion(all[s - 1].conversion);
			}
		}
	}
}


void
VideoPanel::edit_colour_conversion_clicked()
{
	auto vc = _parent->selected_video();

	ContentColourConversionDialog dialog(this, vc.front()->video->yuv());
	dialog.set(vc.front()->video->colour_conversion().get_value_or(PresetColourConversion::all().front().conversion));
	if (dialog.ShowModal() == wxID_OK) {
		for (auto i: vc) {
			i->video->set_colour_conversion(dialog.get());
		}
	} else {
		/* Reset the colour conversion choice */
		film_content_changed(VideoContentProperty::COLOUR_CONVERSION);
	}
}


void
VideoPanel::content_selection_changed()
{
	auto video_sel = _parent->selected_video();

	_frame_type->set_content(video_sel);
	_left_crop->set_content(video_sel);
	_right_crop->set_content(video_sel);
	_top_crop->set_content(video_sel);
	_bottom_crop->set_content(video_sel);

	film_content_changed(ContentProperty::VIDEO_FRAME_RATE);
	film_content_changed(VideoContentProperty::CROP);
	film_content_changed(VideoContentProperty::COLOUR_CONVERSION);
	film_content_changed(VideoContentProperty::FADE_IN);
	film_content_changed(VideoContentProperty::FADE_OUT);
	film_content_changed(VideoContentProperty::RANGE);
	film_content_changed(VideoContentProperty::USE);
	film_content_changed(VideoContentProperty::CUSTOM_RATIO);
	film_content_changed(VideoContentProperty::CUSTOM_SIZE);
	film_content_changed(FFmpegContentProperty::FILTERS);
	film_content_changed(DCPContentProperty::REFERENCE_VIDEO);

	setup_sensitivity();
}


void
VideoPanel::setup_sensitivity()
{
	auto sel = _parent->selected();

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent>(sel.front());
	}

	bool const reference = dcp && dcp->reference_video();

	bool any_use = false;
	for (auto i: _parent->selected_video()) {
		if (i->video && i->video->use()) {
			any_use = true;
		}
	}

	bool const enable = !reference && any_use;

	if (!enable) {
		_frame_type->wrapped()->Enable(false);
		_left_crop->wrapped()->Enable(false);
		_right_crop->wrapped()->Enable(false);
		_top_crop->wrapped()->Enable(false);
		_bottom_crop->wrapped()->Enable(false);
		_fade_in->Enable(false);
		_fade_out->Enable(false);
		_scale_fit->Enable(false);
		_scale_custom->Enable(false);
		_scale_custom_edit->Enable(false);
		_description->Enable(false);
		_colour_conversion->Enable(false);
		_range->Enable(false);
	} else {
		auto video_sel = _parent->selected_video();
		auto ffmpeg_sel = _parent->selected_ffmpeg();
		bool const single = video_sel.size() == 1;

		_frame_type->wrapped()->Enable(true);
		_left_crop->wrapped()->Enable(true);
		_right_crop->wrapped()->Enable(true);
		_top_crop->wrapped()->Enable(true);
		_bottom_crop->wrapped()->Enable(true);
		_fade_in->Enable(!video_sel.empty());
		_fade_out->Enable(!video_sel.empty());
		_scale_fit->Enable(true);
		_scale_custom->Enable(true);
		_scale_custom_edit->Enable(_scale_custom->GetValue());
		_description->Enable(true);
		_colour_conversion->Enable(!video_sel.empty());
		_range->Enable(single && !video_sel.empty() && !dcp);
	}

	auto vc = _parent->selected_video();
	shared_ptr<Content> vcs;
	if (!vc.empty()) {
		vcs = vc.front();
	}

	if (vcs && vcs->video->colour_conversion()) {
		_edit_colour_conversion_button->Enable(!vcs->video->colour_conversion().get().preset());
	} else {
		_edit_colour_conversion_button->Enable(false);
	}
}


void
VideoPanel::fade_in_changed()
{
	auto const hmsf = _fade_in->get();
	for (auto i: _parent->selected_video()) {
		auto const vfr = i->active_video_frame_rate(_parent->film());
		i->video->set_fade_in(dcpomatic::ContentTime(hmsf, vfr).frames_round(vfr));
	}
}


void
VideoPanel::fade_out_changed()
{
	auto const hmsf = _fade_out->get();
	for (auto i: _parent->selected_video()) {
		auto const vfr = i->active_video_frame_rate(_parent->film());
		i->video->set_fade_out(dcpomatic::ContentTime(hmsf, vfr).frames_round(vfr));
	}
}


void
VideoPanel::scale_fit_clicked()
{
	for (auto i: _parent->selected_video()) {
		i->video->set_custom_ratio(optional<float>());
		i->video->set_custom_size(optional<dcp::Size>());
	}

	setup_sensitivity();
}


void
VideoPanel::scale_custom_clicked()
{
	if (!scale_custom_edit_clicked()) {
		_scale_fit->SetValue(true);
	}

	setup_sensitivity();
}


bool
VideoPanel::scale_custom_edit_clicked()
{
	auto vc = _parent->selected_video().front()->video;
	auto size = vc->size();
	DCPOMATIC_ASSERT(size);

	CustomScaleDialog dialog(this, *size, _parent->film()->frame_size(), vc->custom_ratio(), vc->custom_size());
	if (dialog.ShowModal() != wxID_OK) {
		return false;
	}

	for (auto i: _parent->selected_video()) {
		i->video->set_custom_ratio(dialog.custom_ratio());
		i->video->set_custom_size(dialog.custom_size());
	}

	return true;
}


void
VideoPanel::left_right_link_clicked()
{
	if (_left_changed_last) {
		left_crop_changed();
	} else {
		right_crop_changed();
	}
}


void
VideoPanel::top_bottom_link_clicked()
{
	if (_top_changed_last) {
		top_crop_changed();
	} else {
		bottom_crop_changed();
	}
}


void
VideoPanel::left_crop_changed()
{
	_left_changed_last = true;
	if (_left_right_link->GetValue()) {
		for (auto const& i: _parent->selected_video()) {
			i->video->set_right_crop(i->video->requested_left_crop());
		}
	}
}


void
VideoPanel::right_crop_changed()
{
	_left_changed_last = false;
	if (_left_right_link->GetValue()) {
		for (auto const& i: _parent->selected_video()) {
			i->video->set_left_crop(i->video->requested_right_crop());
		}
	}
}


void
VideoPanel::top_crop_changed()
{
	_top_changed_last = true;
	if (_top_bottom_link->GetValue()) {
		for (auto i: _parent->selected_video()) {
			i->video->set_bottom_crop(i->video->requested_top_crop());
		}
	}
}


void
VideoPanel::bottom_crop_changed()
{
	_top_changed_last = false;
	if (_top_bottom_link->GetValue()) {
		for(auto i: _parent->selected_video()) {
			i->video->set_top_crop(i->video->requested_bottom_crop());
		}
	}
}


