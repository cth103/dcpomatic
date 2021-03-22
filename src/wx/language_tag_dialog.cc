/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "language_tag_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include <wx/listctrl.h>
#include <wx/wx.h>
#include <dcp/language_tag.h>


using std::vector;


LanguageTagDialog::LanguageTagDialog (wxWindow* parent, dcp::LanguageTag tag)
	: wxDialog (parent, wxID_ANY, _("Language Tag"))
{
	_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize(600, 700), wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
	_list->AppendColumn ("", wxLIST_FORMAT_LEFT, 400);
	_list->AppendColumn ("", wxLIST_FORMAT_LEFT, 150);
	auto add = new wxButton (this, wxID_ANY, _("Add language..."));

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (_list, 0, wxALL, DCPOMATIC_SIZER_GAP);
	overall_sizer->Add (add, 0, wxALL, DCPOMATIC_SIZER_GAP);

	auto buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	for (auto const& i: dcp::dcnc_tags()) {
		_presets.push_back (dcp::LanguageTag(i.first));
	}

	std::sort (_presets.begin(), _presets.end(), [](dcp::LanguageTag const& i, dcp::LanguageTag const& j) {
		return i.description() < j.description();
	});

	_custom = Config::instance()->custom_languages ();

	populate_list ();

	set (tag);

	add->Bind (wxEVT_BUTTON, boost::bind(&LanguageTagDialog::add_language, this));
}


void
LanguageTagDialog::add_language ()
{
	auto full = new FullLanguageTagDialog (GetParent());
	auto r = full->ShowModal ();
	if (r == wxID_OK) {
		Config::instance()->add_custom_language (full->get());
		set (full->get());
	}
	full->Destroy ();
}


void
LanguageTagDialog::populate_list ()
{
	_list->DeleteAllItems ();

	auto add = [this](vector<dcp::LanguageTag> const& tags) {
		for (auto const& i: tags) {
			wxListItem it;
			it.SetId (_list->GetItemCount());
			it.SetColumn (0);
			it.SetText (std_to_wx(i.description()));
			_list->InsertItem (it);
			it.SetColumn (1);
			it.SetText (std_to_wx(i.to_string()));
			_list->SetItem (it);
		}
	};

	add (_presets);
	add (_custom);
}


void
LanguageTagDialog::set (dcp::LanguageTag tag)
{
	size_t selection = 0;

	auto iter = find(_presets.begin(), _presets.end(), tag);
	if (iter == _presets.end()) {
		iter = find(_custom.begin(), _custom.end(), tag);
		if (iter == _custom.end()) {
			_custom.push_back (tag);
			selection = _presets.size() + _custom.size() - 1;
			populate_list ();
			_list->EnsureVisible (_list->GetItemCount() - 1);
		} else {
			selection = _presets.size() + std::distance(_custom.begin(), iter);
		}
	} else {
		selection = std::distance(_presets.begin(), iter);
	}

	_list->SetItemState (selection, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	_list->EnsureVisible (selection);
}


dcp::LanguageTag
LanguageTagDialog::get () const
{
	auto selected = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	DCPOMATIC_ASSERT (selected >= 0);

	if (selected < static_cast<long>(_presets.size())) {
		return _presets[selected];
	}

	selected -= _presets.size();

	DCPOMATIC_ASSERT (selected < static_cast<long>(_custom.size()));
	return _custom[selected];
}

