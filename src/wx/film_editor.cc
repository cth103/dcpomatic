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


/** @file src/wx/film_editor.cc
 *  @brief FilmEditor class.
 */


#include "content_panel.h"
#include "dcp_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "lib/content.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/notebook.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::list;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


FilmEditor::FilmEditor(wxWindow* parent, FilmViewer& viewer)
	: wxPanel (parent)
{
	auto s = new wxBoxSizer (wxVERTICAL);

	_notebook = new wxNotebook(this, wxID_ANY);
	s->Add(_notebook, 1, wxEXPAND);

	_content_panel = new ContentPanel(_notebook, _film, viewer);
	_notebook->AddPage(_content_panel->window(), _("Content"), true);
	_dcp_panel = new DCPPanel(_notebook, _film, viewer);
	_notebook->AddPage(_dcp_panel->panel (), _("DCP"), false);

	_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, boost::bind(&FilmEditor::page_changed, this, _1));

	JobManager::instance()->ActiveJobsChanged.connect (
		bind(&FilmEditor::active_jobs_changed, this, _2)
		);

	set_film (shared_ptr<Film> ());
	SetSizerAndFit (s);
}


void
FilmEditor::page_changed(wxBookCtrlEvent& ev)
{
	/* One of these events arrives early on with GetOldSelection() being a non-existent tab,
	 * and we want to ignore that.
	 */
	if (_film && ev.GetOldSelection() < 2) {
		_film->set_ui_state("FilmEditorTab", ev.GetSelection() == 0 ? "content" : "dcp");
	}
}


/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_change(ChangeType type, FilmProperty p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	ensure_ui_thread ();

	if (!_film) {
		return;
	}

	_content_panel->film_changed (p);
	_dcp_panel->film_changed (p);

	if (p == FilmProperty::CONTENT && !_film->content().empty()) {
		/* Select newly-added content */
		_content_panel->set_selection (_film->content().back ());
	}
}


void
FilmEditor::film_content_change (ChangeType type, int property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	ensure_ui_thread ();

	if (!_film) {
		/* We call this method ourselves (as well as using it as a signal handler)
		   so _film can be 0.
		*/
		return;
	}

	_content_panel->film_content_changed (property);
	_dcp_panel->film_content_changed (property);
}


/** Sets the Film that we are editing */
void
FilmEditor::set_film (shared_ptr<Film> film)
{
	set_general_sensitivity (film != nullptr);

	if (_film == film) {
		return;
	}

	_film = film;

	_content_panel->set_film (_film);
	_dcp_panel->set_film (_film);

	if (!_film) {
		return;
	}

	_film->Change.connect (bind(&FilmEditor::film_change, this, _1, _2));
	_film->ContentChange.connect (bind(&FilmEditor::film_content_change, this, _1, _3));

	if (!_film->content().empty()) {
		_content_panel->set_selection (_film->content().front());
	}

	auto tab = _film->ui_state("FilmEditorTab").get_value_or("content");
	if (tab == "content") {
		_notebook->SetSelection(0);
	} else if (tab == "dcp") {
		_notebook->SetSelection(1);
	}
}


void
FilmEditor::set_general_sensitivity (bool s)
{
	_content_panel->set_general_sensitivity (s);
	_dcp_panel->set_general_sensitivity (s);
}


void
FilmEditor::active_jobs_changed (optional<string> j)
{
	set_general_sensitivity (!j);
}


void
FilmEditor::first_shown ()
{
	_content_panel->first_shown ();
}

