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


#include "full_language_tag_dialog.h"
#include "language_subtag_panel.h"
#include "subtag_list_ctrl.h"
#include "wx_util.h"
#include "lib/dcpomatic_assert.h"
#include <dcp/language_tag.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <iterator>
#include <string>
#include <vector>


using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


FullLanguageTagDialog::FullLanguageTagDialog (wxWindow* parent, dcp::LanguageTag tag)
	: wxDialog (parent, wxID_ANY, _("Language Tag"), wxDefaultPosition, wxSize(-1, 500))
{
	_current_tag_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
	_current_tag_list->AppendColumn({}, wxLIST_FORMAT_LEFT, 200);
	_current_tag_list->AppendColumn({}, wxLIST_FORMAT_LEFT, 400);

	auto button_sizer = new wxBoxSizer (wxVERTICAL);
	_add_script = new wxButton(this, wxID_ANY, "Add script");
	button_sizer->Add (_add_script, 0, wxTOP | wxBOTTOM | wxEXPAND, 2);
	_add_region = new wxButton(this, wxID_ANY, "Add region");
	button_sizer->Add (_add_region, 0, wxTOP | wxBOTTOM | wxEXPAND, 2);
	_add_variant = new wxButton(this, wxID_ANY, "Add variant");
	button_sizer->Add (_add_variant, 0, wxTOP | wxBOTTOM | wxEXPAND, 2);
	_add_external = new wxButton(this, wxID_ANY, "Add external");
	button_sizer->Add (_add_external, 0, wxTOP | wxBOTTOM | wxEXPAND, 2);
	_remove = new wxButton(this, wxID_ANY, "Remove");
	button_sizer->Add (_remove, 0, wxTOP | wxBOTTOM | wxEXPAND, 2);

	_choose_subtag_panel = new LanguageSubtagPanel (this);
	_choose_subtag_panel->set (dcp::LanguageTag::SubtagType::LANGUAGE, "");

	auto ltor_sizer = new wxBoxSizer (wxHORIZONTAL);
	ltor_sizer->Add (_current_tag_list, 1, wxALL, 8);
	ltor_sizer->Add (button_sizer, 0, wxALL, 8);
	ltor_sizer->Add (_choose_subtag_panel, 1, wxALL, 8);

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (ltor_sizer, 0);

	auto buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	set (tag);

	_add_script->Bind (wxEVT_BUTTON, boost::bind(&FullLanguageTagDialog::add_to_current_tag, this, dcp::LanguageTag::SubtagType::SCRIPT, boost::optional<dcp::LanguageTag::SubtagData>()));
	_add_region->Bind (wxEVT_BUTTON, boost::bind(&FullLanguageTagDialog::add_to_current_tag, this, dcp::LanguageTag::SubtagType::REGION, boost::optional<dcp::LanguageTag::SubtagData>()));
	_add_variant->Bind (wxEVT_BUTTON, boost::bind(&FullLanguageTagDialog::add_to_current_tag, this, dcp::LanguageTag::SubtagType::VARIANT, boost::optional<dcp::LanguageTag::SubtagData>()));
	_add_external->Bind (wxEVT_BUTTON, boost::bind(&FullLanguageTagDialog::add_to_current_tag, this, dcp::LanguageTag::SubtagType::EXTLANG, boost::optional<dcp::LanguageTag::SubtagData>()));
	_remove->Bind (wxEVT_BUTTON, boost::bind(&FullLanguageTagDialog::remove_from_current_tag, this));
	_choose_subtag_panel->SelectionChanged.connect(bind(&FullLanguageTagDialog::chosen_subtag_changed, this, _1));
	_choose_subtag_panel->SearchChanged.connect(bind(&FullLanguageTagDialog::search_changed, this, _1));
	_current_tag_list->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind(&FullLanguageTagDialog::current_tag_selection_changed, this));
	_current_tag_list->Bind (wxEVT_LIST_ITEM_DESELECTED, boost::bind(&FullLanguageTagDialog::current_tag_selection_changed, this));
}


void
FullLanguageTagDialog::remove_from_current_tag ()
{
	auto selected = _current_tag_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected <= 0) {
		return;
	}

	_current_tag_subtags.erase (_current_tag_subtags.begin() + selected);
	_current_tag_list->DeleteItem (selected);

	_current_tag_list->SetItemState (min(selected, _current_tag_list->GetItemCount() - 1L), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

	setup_sensitivity ();
	current_tag_selection_changed ();
}


dcp::LanguageTag FullLanguageTagDialog::get () const
{
	dcp::LanguageTag tag;

	vector<dcp::LanguageTag::VariantSubtag> variants;
	vector<dcp::LanguageTag::ExtlangSubtag> extlangs;

	for (auto i: _current_tag_subtags) {
		if (!i.subtag) {
			continue;
		}
		switch (i.type) {
			case dcp::LanguageTag::SubtagType::LANGUAGE:
				tag.set_language (i.subtag->subtag);
				break;
			case dcp::LanguageTag::SubtagType::SCRIPT:
				tag.set_script (i.subtag->subtag);
				break;
			case dcp::LanguageTag::SubtagType::REGION:
				tag.set_region (i.subtag->subtag);
				break;
			case dcp::LanguageTag::SubtagType::VARIANT:
				variants.push_back (i.subtag->subtag);
				break;
			case dcp::LanguageTag::SubtagType::EXTLANG:
				extlangs.push_back (i.subtag->subtag);
				break;
		}
	}

	tag.set_variants (variants);
	tag.set_extlangs (extlangs);
	return tag;
}


void
FullLanguageTagDialog::set (dcp::LanguageTag tag)
{
	_current_tag_subtags.clear ();
	_current_tag_list->DeleteAllItems ();

	bool have_language = false;
	for (auto const& i: tag.subtags()) {
		add_to_current_tag (i.first, i.second);
		if (i.first == dcp::LanguageTag::SubtagType::LANGUAGE) {
			have_language = true;
		}
	}

	if (!have_language) {
		add_to_current_tag (dcp::LanguageTag::SubtagType::LANGUAGE, dcp::LanguageTag::SubtagData("en", "English"));
	}
}


string FullLanguageTagDialog::subtag_type_name (dcp::LanguageTag::SubtagType type)
{
	switch (type) {
		case dcp::LanguageTag::SubtagType::LANGUAGE:
			return "Language";
		case dcp::LanguageTag::SubtagType::SCRIPT:
			return "Script";
		case dcp::LanguageTag::SubtagType::REGION:
			return "Region";
		case dcp::LanguageTag::SubtagType::VARIANT:
			return "Variant";
		case dcp::LanguageTag::SubtagType::EXTLANG:
			return "External";
	}

	return {};
}


void
FullLanguageTagDialog::search_changed (string search)
{
	long int selected = _current_tag_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected >= 0) {
		_current_tag_subtags[selected].last_search = search;
	}
}


void
FullLanguageTagDialog::add_to_current_tag (dcp::LanguageTag::SubtagType type, optional<dcp::LanguageTag::SubtagData> subtag)
{
	_current_tag_subtags.push_back (Subtag(type, subtag));
	wxListItem it;
	it.SetId (_current_tag_list->GetItemCount());
	it.SetColumn (0);
	it.SetText(std_to_wx(subtag_type_name(type)));
	_current_tag_list->InsertItem (it);
	it.SetColumn (1);
	if (subtag) {
		it.SetText(std_to_wx(subtag->description));
	} else {
		it.SetText ("Select...");
	}
	_current_tag_list->SetItem (it);
	_current_tag_list->SetItemState (_current_tag_list->GetItemCount() - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	_choose_subtag_panel->set (type, "");
	setup_sensitivity ();
	current_tag_selection_changed ();
}


void
FullLanguageTagDialog::current_tag_selection_changed ()
{
	auto selected = _current_tag_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected >= 0) {
		_choose_subtag_panel->Enable (true);
		_choose_subtag_panel->set (_current_tag_subtags[selected].type, _current_tag_subtags[selected].last_search, _current_tag_subtags[selected].subtag);
	} else {
		_choose_subtag_panel->Enable (false);
	}
}


void
FullLanguageTagDialog::chosen_subtag_changed (optional<dcp::LanguageTag::SubtagData> selection)
{
	if (!selection) {
		return;
	}

	auto selected = _current_tag_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected >= 0) {
		_current_tag_subtags[selected].subtag = *selection;
		_current_tag_list->SetItem(selected, 0, std_to_wx(subtag_type_name(_current_tag_subtags[selected].type)));
		_current_tag_list->SetItem(selected, 1, std_to_wx(selection->description));
	}

	setup_sensitivity ();
}

void
FullLanguageTagDialog::setup_sensitivity ()
{
	_add_script->Enable ();
	_add_region->Enable ();
	_add_variant->Enable ();
	_add_external->Enable ();
	for (auto const& i: _current_tag_subtags) {
		switch (i.type) {
			case dcp::LanguageTag::SubtagType::SCRIPT:
				_add_script->Enable (false);
				break;
			case dcp::LanguageTag::SubtagType::REGION:
				_add_region->Enable (false);
				break;
			case dcp::LanguageTag::SubtagType::VARIANT:
				_add_variant->Enable (false);
				break;
			case dcp::LanguageTag::SubtagType::EXTLANG:
				_add_external->Enable (false);
				break;
			default:
				break;
		}
	}
	auto selected = _current_tag_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	_remove->Enable (selected > 0);
}

