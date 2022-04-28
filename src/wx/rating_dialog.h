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


#include <dcp/rating.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class wxChoice;
class wxListView;
class wxNotebook;
class wxSearchCtrl;


class RatingDialogPage : public wxPanel
{
public:
	RatingDialogPage (wxNotebook* notebook);
	virtual dcp::Rating get () const = 0;
	virtual bool set (dcp::Rating rating) = 0;

	/** Emitted when the page has been changed, the parameter being true if OK
	 *  should now be enabled in the main dialogue.
	 */
	boost::signals2::signal<void (bool)> Changed;
};


class StandardRatingDialogPage : public RatingDialogPage
{
public:
	StandardRatingDialogPage (wxNotebook* notebook);

	dcp::Rating get () const override;
	bool set (dcp::Rating rating) override;

private:
	void search_changed ();
	void found_systems_view_selection_changed ();
	void update_found_system_selection ();

	wxSearchCtrl* _search;
	wxListView* _found_systems_view;
	boost::optional<dcp::RatingSystem> _selected_system;
	wxChoice* _rating;
	std::vector<dcp::RatingSystem> _found_systems;
};


class CustomRatingDialogPage : public RatingDialogPage
{
public:
	CustomRatingDialogPage (wxNotebook* notebook);

	dcp::Rating get () const override;
	bool set (dcp::Rating rating) override;

private:
	void changed ();

	wxTextCtrl* _agency;
	wxTextCtrl* _rating;
};


class RatingDialog : public wxDialog
{
public:
	RatingDialog (wxWindow* parent);

	void set (dcp::Rating r);
	boost::optional<dcp::Rating> get () const;

private:
	void setup_sensitivity (bool ok_valid);
	void page_changed ();

	wxNotebook* _notebook;

	StandardRatingDialogPage* _standard_page;
	CustomRatingDialogPage* _custom_page;
	RatingDialogPage* _active_page;
};

