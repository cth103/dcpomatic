/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_properties_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/content.h"
#include "lib/video_content.h"
#include <boost/algorithm/string.hpp>


using std::dynamic_pointer_cast;
using std::list;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


ContentPropertiesDialog::ContentPropertiesDialog(wxWindow* parent, shared_ptr<const Film> film, shared_ptr<Content> content)
	: wxDialog(parent, wxID_ANY, _("Content Properties"), wxDefaultPosition, wxSize(640, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	_scroll = new wxScrolledWindow(this);
	_scroll->SetMinSize(wxSize(740, 600));
	_scroll->EnableScrolling(false, true);
	_scroll->SetScrollRate(0, 32);

	_table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_X_GAP);
	_table->AddGrowableCol(1, 1);
	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(_table, 1, wxEXPAND);
	_scroll->SetSizer(sizer);

	map<UserProperty::Category, list<UserProperty>> grouped;
	for (auto i: content->user_properties(film)) {
		if (grouped.find(i.category) == grouped.end()) {
			grouped[i.category] = list<UserProperty>();
		}
		grouped[i.category].push_back(i);
	}

	maybe_add_group(grouped, UserProperty::GENERAL);
	maybe_add_group(grouped, UserProperty::VIDEO);
	maybe_add_group(grouped, UserProperty::AUDIO);
	maybe_add_group(grouped, UserProperty::LENGTH);

	/* Nasty hack to stop the bottom property being cut off on Windows / OS X */
	_table->Add(new wxStaticText(_scroll, wxID_ANY, wxT("")), 0, 0);
	_table->Add(new wxStaticText(_scroll, wxID_ANY, wxT("")), 0, 0);

	auto overall_sizer = new wxBoxSizer(wxVERTICAL);
	overall_sizer->Add(_scroll, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	SetSizerAndFit(overall_sizer);
}


void
ContentPropertiesDialog::maybe_add_group(map<UserProperty::Category, list<UserProperty>> const & groups, UserProperty::Category category)
{
	auto i = groups.find(category);
	if (i == groups.end()) {
		return;
	}

	wxString category_name;
	switch (i->first) {
	case UserProperty::GENERAL:
		category_name = _("General");
		break;
	case UserProperty::VIDEO:
		category_name = _("Video");
		break;
	case UserProperty::AUDIO:
		category_name = _("Audio");
		break;
	case UserProperty::LENGTH:
		category_name = _("Length");
		break;
	}

	auto m = new StaticText(_scroll, category_name);
	wxFont font(*wxNORMAL_FONT);
	font.SetWeight(wxFONTWEIGHT_BOLD);
	m->SetFont(font);

	_table->AddSpacer(0);
	_table->AddSpacer(0);
	_table->Add(m, 1);
	_table->AddSpacer(0);

	vector<string> sub_headings;
	for (auto j: i->second) {
		if (j.sub_heading) {
			sub_headings.push_back(*j.sub_heading);
		}
	}

	std::sort(sub_headings.begin(), sub_headings.end());
	auto last = std::unique(sub_headings.begin(), sub_headings.end());
	sub_headings.erase(last, sub_headings.end());

	auto add_sub_heading = [&](optional<string> sub_heading) {
		if (sub_heading) {
			auto heading = add_label_to_sizer(_table, _scroll, std_to_wx(*sub_heading), true, 0, wxALIGN_TOP);
			wxFont font(*wxNORMAL_FONT);
			font.SetStyle(wxFONTSTYLE_ITALIC);
			heading->SetFont(font);
			_table->AddSpacer(0);
		}
		for (auto j: i->second) {
			if (j.sub_heading == sub_heading) {
				add_label_to_sizer(_table, _scroll, std_to_wx(j.key), true, 0, wxALIGN_TOP);
				_table->Add(new StaticText(_scroll, std_to_wx(j.value + " " + j.unit)), 1);
			}
		}
	};

	add_sub_heading(boost::none);
	for (auto const& h: sub_headings) {
		add_sub_heading(h);
	}
}
