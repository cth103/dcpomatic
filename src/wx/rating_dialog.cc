/*
    Copyright (C) 2019-2022 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_spin_ctrl.h"
#include "rating_dialog.h"
#include "wx_util.h"
#include <unicode/unistr.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/srchctrl.h>


using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


RatingDialog::RatingDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Rating"))
{
	_notebook = new wxNotebook (this, wxID_ANY);

	_standard_page = new StandardRatingDialogPage (_notebook);
	_custom_page = new CustomRatingDialogPage (_notebook);

	_notebook->AddPage (_standard_page, _("Standard"));
	_notebook->AddPage (_custom_page, _("Custom"));

	_active_page = _standard_page;

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (_notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, boost::bind(&RatingDialog::page_changed, this));

	_standard_page->Changed.connect(boost::bind(&RatingDialog::setup_sensitivity, this, _1));
	_custom_page->Changed.connect(boost::bind(&RatingDialog::setup_sensitivity, this, _1));
}


void
RatingDialog::page_changed ()
{
	if (_notebook->GetSelection() == 0) {
		_active_page = _standard_page;
	} else {
		_active_page = _custom_page;
	}
}


void
RatingDialog::set (dcp::Rating rating)
{
	if (_standard_page->set(rating)) {
		_notebook->SetSelection(0);
	} else {
		_custom_page->set(rating);
		_notebook->SetSelection(1);
	}
}


dcp::Rating
RatingDialog::get () const
{
	return _active_page->get();
}


void
RatingDialog::setup_sensitivity (bool ok_valid)
{
	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Enable (ok_valid);
	}
}


RatingDialogPage::RatingDialogPage (wxNotebook* notebook)
	: wxPanel (notebook, wxID_ANY)
{

}


StandardRatingDialogPage::StandardRatingDialogPage (wxNotebook* notebook)
	: RatingDialogPage (notebook)
{
	_search = new wxSearchCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, search_ctrl_height()));
#ifndef __WXGTK3__
	/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
	_search->ShowCancelButton (true);
#endif

	_found_systems_view = new wxListView (this, wxID_ANY, wxDefaultPosition, wxSize(600, 400), wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
	_found_systems_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 150);
	_found_systems_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 50);
	_found_systems_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 400);
	_rating = new wxChoice (this, wxID_ANY);

	auto sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_sizer (sizer, this, _("Agency"), true, 0, wxALIGN_CENTER_VERTICAL);
	sizer->Add (_search, 0, wxEXPAND, DCPOMATIC_SIZER_Y_GAP);

	sizer->AddSpacer (0);
	sizer->Add (_found_systems_view, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_sizer (sizer, this, _("Rating"), true, 0, wxALIGN_CENTER_VERTICAL);
	sizer->Add (_rating, 1, wxEXPAND);

	auto pad_sizer = new wxBoxSizer (wxVERTICAL);
	pad_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	SetSizerAndFit (pad_sizer);

	_search->Bind (wxEVT_TEXT, boost::bind(&StandardRatingDialogPage::search_changed, this));
	_found_systems_view->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind(&StandardRatingDialogPage::found_systems_view_selection_changed, this));
	_found_systems_view->Bind (wxEVT_LIST_ITEM_DESELECTED, boost::bind(&StandardRatingDialogPage::found_systems_view_selection_changed, this));

	search_changed ();
}


/** The user clicked something different in the list of systems found by the search */
void
StandardRatingDialogPage::found_systems_view_selection_changed ()
{
	auto selected_index = _found_systems_view->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected_index < 0 || selected_index >= static_cast<int>(_found_systems.size())) {
		_selected_system = boost::none;
	} else {
		_selected_system = _found_systems[selected_index];
	}

	/* Update the ratings dropdown */
	vector<wxString> items;
	if (_selected_system) {
		for (auto rating: _selected_system->ratings) {
			items.push_back(std_to_wx(rating.label));
		}
	}

	_rating->Set(items);

	if (!items.empty()) {
		_rating->SetSelection(0);
	}

	Changed (static_cast<bool>(_selected_system));
}


void
StandardRatingDialogPage::search_changed ()
{
	_found_systems_view->DeleteAllItems();
	_found_systems.clear();

	icu::UnicodeString term(wx_to_std(_search->GetValue()).c_str(), "UTF-8");
	term = term.toLower();

	int N = 0;
	for (auto const& system: dcp::rating_systems()) {
		icu::UnicodeString name(system.name.c_str(), "UTF-8");
		name = name.toLower();
		icu::UnicodeString country_and_region_names(system.country_and_region_names.c_str(), "UTF-8");
		country_and_region_names = country_and_region_names.toLower();
		icu::UnicodeString country_code(system.country_code.c_str(), "UTF-8");
		country_code = country_code.toLower();
		if (term.isEmpty() || name.indexOf(term) != -1 || country_and_region_names.indexOf(term) != -1 || country_code.indexOf(term) != -1) {
			wxListItem item;
			item.SetId(N);
			_found_systems_view->InsertItem(item);
			_found_systems_view->SetItem(N, 0, std_to_wx(system.name));
			_found_systems_view->SetItem(N, 1, std_to_wx(system.country_code));
			_found_systems_view->SetItem(N, 2, std_to_wx(system.country_and_region_names));
			_found_systems.push_back(system);
			++N;
		}
	}

	update_found_system_selection ();
}


/** Reflect _selected_system in the current _found_systems_view */
void
StandardRatingDialogPage::update_found_system_selection ()
{
	if (!_selected_system) {
		for (auto i = 0; i < _found_systems_view->GetItemCount(); ++i) {
			_found_systems_view->Select(i, false);
		}
		return;
	}

	int index = 0;
	for (auto const& system: _found_systems) {
		bool const selected = system.agency == _selected_system->agency;
		_found_systems_view->Select(index, selected);
		if (selected) {
			_found_systems_view->EnsureVisible(index);
		}
		++index;
	}
}


bool
StandardRatingDialogPage::set (dcp::Rating rating)
{
	_selected_system = boost::none;
	for (auto const& system: dcp::rating_systems()) {
		if (system.agency == rating.agency) {
			_selected_system = system;
			break;
		}
	}

	if (!_selected_system) {
		return false;
	}

	update_found_system_selection ();

	int rating_index = 0;
	for (auto const& possible_rating: _selected_system->ratings) {
		if (possible_rating.label == rating.label) {
			_rating->SetSelection (rating_index);
			return true;
		}
		++rating_index;
	}

	return false;
}


dcp::Rating
StandardRatingDialogPage::get () const
{
	DCPOMATIC_ASSERT (_selected_system);
	auto selected_rating = _rating->GetSelection();
	DCPOMATIC_ASSERT (selected_rating >= 0);
	DCPOMATIC_ASSERT (selected_rating < static_cast<int>(_selected_system->ratings.size()));
	return dcp::Rating(_selected_system->agency, _selected_system->ratings[selected_rating].label);
}


CustomRatingDialogPage::CustomRatingDialogPage (wxNotebook* notebook)
	: RatingDialogPage (notebook)
{
	auto sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	_agency = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1));
	_rating = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1));

	add_label_to_sizer (sizer, this, _("Agency"), true, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	sizer->Add (_agency, 1, wxEXPAND);
	add_label_to_sizer (sizer, this, _("Rating"), true, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	sizer->Add (_rating, 1, wxEXPAND);

	auto pad_sizer = new wxBoxSizer (wxVERTICAL);
	pad_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	SetSizerAndFit (pad_sizer);

	_agency->Bind(wxEVT_TEXT, boost::bind(&CustomRatingDialogPage::changed, this));
	_rating->Bind(wxEVT_TEXT, boost::bind(&CustomRatingDialogPage::changed, this));
}


void
CustomRatingDialogPage::changed ()
{
	Changed (!_agency->IsEmpty() && !_rating->IsEmpty());
}


dcp::Rating
CustomRatingDialogPage::get () const
{
	return dcp::Rating(wx_to_std(_agency->GetValue()), wx_to_std(_rating->GetValue()));
}


bool
CustomRatingDialogPage::set (dcp::Rating rating)
{
	_agency->SetValue(std_to_wx(rating.agency));
	_rating->SetValue(std_to_wx(rating.label));
	return true;
}

