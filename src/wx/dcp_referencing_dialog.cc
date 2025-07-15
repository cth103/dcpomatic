/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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
#include "dcp_referencing_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/types.h"
#include <wx/gbsizer.h>
#include <wx/wx.h>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


DCPReferencingDialog::DCPReferencingDialog(wxWindow* parent, shared_ptr<const Film> film)
	: wxDialog(parent, wxID_ANY, _("Version file (VF) setup"))
	, _film(film)
	, _dcp_grid(new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP))
	, _overall_sizer(new wxBoxSizer(wxVERTICAL))
{
	_film_connection = film->Change.connect(boost::bind(&DCPReferencingDialog::film_changed, this, _1, _2));
	_film_content_connection = film->ContentChange.connect(boost::bind(&DCPReferencingDialog::film_content_changed, this, _1, _2));

	_overall_sizer->Add(_dcp_grid, 1, wxALL, DCPOMATIC_DIALOG_BORDER);
	SetSizer(_overall_sizer);

	auto buttons = CreateSeparatedButtonSizer(wxOK);
	if (buttons) {
		_overall_sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	setup();
}


void
DCPReferencingDialog::film_changed(ChangeType type, FilmProperty property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (
		property == FilmProperty::INTEROP ||
		property == FilmProperty::RESOLUTION ||
		property == FilmProperty::CONTAINER ||
		property == FilmProperty::REEL_TYPE ||
		property == FilmProperty::VIDEO_FRAME_RATE ||
		property == FilmProperty::AUDIO_CHANNELS ||
		property == FilmProperty::CONTENT) {
		setup();
	}
}


void
DCPReferencingDialog::film_content_changed(ChangeType type, int property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (
		property != DCPContentProperty::REFERENCE_VIDEO &&
		property != DCPContentProperty::REFERENCE_AUDIO &&
		property != DCPContentProperty::REFERENCE_TEXT) {
		setup();
	}
}


void
DCPReferencingDialog::setup()
{
	_dcps.clear();
	_dcp_grid->Clear(true);

	int row = 0;
	auto text = new StaticText(this, _("Refer to"));
	wxFont font(*wxNORMAL_FONT);
	font.SetWeight(wxFONTWEIGHT_BOLD);
	text->SetFont(font);
	_dcp_grid->Add(text, wxGBPosition(row, 1), wxGBSpan(1, 4), wxALIGN_CENTER);
	++row;

	int column = 0;
	for (auto const& sub_heading: { _("OV DCP"), _("Picture"), _("Sound"), _("Subtitles"), _("Closed captions") }) {
		auto text = new StaticText(this, sub_heading);
		wxFont font(*wxNORMAL_FONT);
		font.SetWeight(wxFONTWEIGHT_BOLD);
		text->SetFont(font);
		_dcp_grid->Add(text, wxGBPosition(row, column), wxDefaultSpan, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		++column;
	}
	++row;

	auto const all_parts = { Part::VIDEO, Part::AUDIO, Part::SUBTITLES, Part::CLOSED_CAPTIONS };

	for (auto& content: _film->content()) {
		auto dcp_content = dynamic_pointer_cast<DCPContent>(content);
		if (!dcp_content) {
			continue;
		}

		DCP record;
		record.content = dcp_content;
		_dcp_grid->Add(new StaticText(this, std_to_wx(dcp_content->name())), wxGBPosition(row, 0));
		column = 1;
		for (auto const& part: all_parts) {
			record.check_box[part] = new CheckBox(this, {});
			switch (part) {
			case Part::VIDEO:
				record.check_box[part]->set(dcp_content->reference_video());
				break;
			case Part::AUDIO:
				record.check_box[part]->set(dcp_content->reference_audio());
				break;
			case Part::SUBTITLES:
				record.check_box[part]->set(dcp_content->reference_text(TextType::OPEN_SUBTITLE));
				break;
			case Part::CLOSED_CAPTIONS:
				record.check_box[part]->set(dcp_content->reference_text(TextType::CLOSED_CAPTION));
				break;
			default:
				DCPOMATIC_ASSERT(false);
			}
			record.check_box[part]->bind(&DCPReferencingDialog::checkbox_changed, this, weak_ptr<DCPContent>(dcp_content), record.check_box[part], part);
			_dcp_grid->Add(record.check_box[part], wxGBPosition(row, column++), wxDefaultSpan, wxALIGN_CENTER);
		}
		++row;

		auto add_problem = [this, &row](wxString const& cannot, string why_not) {
			auto reason = new StaticText(this, cannot + char_to_wx(": ") + std_to_wx(why_not));
			wxFont font(*wxNORMAL_FONT);
			font.SetStyle(wxFONTSTYLE_ITALIC);
			reason->SetFont(font);
			_dcp_grid->Add(reason, wxGBPosition(row, 0), wxGBSpan(1, 5), wxLEFT, DCPOMATIC_SIZER_X_GAP * 4);
			++row;
		};

		string why_not;

		if (!dcp_content->can_reference_anything(_film, why_not)) {
			for (auto const& part: all_parts) {
				record.check_box[part]->Enable(false);
			}
			add_problem(_("Cannot reference this DCP"), why_not);
		} else {
			if (!dcp_content->can_reference_video(_film, why_not)) {
				record.check_box[Part::VIDEO]->Enable(false);
				if (dcp_content->video) {
					add_problem(_("Cannot reference this DCP's video"), why_not);
				}
			}

			if (!dcp_content->can_reference_audio(_film, why_not)) {
				record.check_box[Part::AUDIO]->Enable(false);
				if (dcp_content->audio) {
					add_problem(_("Cannot reference this DCP's audio"), why_not);
				}
			}

			if (!dcp_content->can_reference_text(_film, TextType::OPEN_SUBTITLE, why_not)) {
				record.check_box[Part::SUBTITLES]->Enable(false);
				if (dcp_content->text_of_original_type(TextType::OPEN_SUBTITLE)) {
					add_problem(_("Cannot reference this DCP's subtitles"), why_not);
				}
			}

			if (!dcp_content->can_reference_text(_film, TextType::CLOSED_CAPTION, why_not)) {
				record.check_box[Part::CLOSED_CAPTIONS]->Enable(false);
				if (dcp_content->text_of_original_type(TextType::CLOSED_CAPTION)) {
					add_problem(_("Cannot reference this DCP's closed captions"), why_not);
				}
			}
		}

		_dcps.push_back(record);
	}

	_dcp_grid->Layout();
	_overall_sizer->Layout();
	_overall_sizer->SetSizeHints(this);
}


void
DCPReferencingDialog::checkbox_changed(weak_ptr<DCPContent> weak_content, CheckBox* check_box, Part part)
{
	auto content = weak_content.lock();
	if (!content) {
		return;
	}

	switch (part) {
	case Part::VIDEO:
		content->set_reference_video(check_box->get());
		break;
	case Part::AUDIO:
		content->set_reference_audio(check_box->get());
		break;
	case Part::SUBTITLES:
		content->set_reference_text(TextType::OPEN_SUBTITLE, check_box->get());
		break;
	case Part::CLOSED_CAPTIONS:
		content->set_reference_text(TextType::CLOSED_CAPTION, check_box->get());
		break;
	default:
		DCPOMATIC_ASSERT(false);
	}
}

