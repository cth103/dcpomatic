/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "wx_util.h"
#include "film_editor.h"
#include "dcp_panel.h"
#include "content_panel.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/content.h"
#include "lib/dcp_content.h"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::string;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

FilmEditor::FilmEditor (wxWindow* parent, weak_ptr<FilmViewer> viewer)
	: wxPanel (parent)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);

	_main_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_main_notebook, 1);

	_content_panel = new ContentPanel (_main_notebook, _film, viewer);
	_main_notebook->AddPage (_content_panel->window(), _("Content"), true);
	_dcp_panel = new DCPPanel (_main_notebook, _film);
	_main_notebook->AddPage (_dcp_panel->panel (), _("DCP"), false);

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmEditor::active_jobs_changed, this, _2)
		);

	set_film (shared_ptr<Film> ());
	SetSizerAndFit (s);
}


/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_change (ChangeType type, Film::Property p)
{
	if (type != CHANGE_TYPE_DONE) {
		return;
	}

	ensure_ui_thread ();

	if (!_film) {
		return;
	}

	_content_panel->film_changed (p);
	_dcp_panel->film_changed (p);

	if (p == Film::CONTENT && !_film->content().empty ()) {
		/* Select newly-added content */
		_content_panel->set_selection (_film->content().back ());
	}
}

void
FilmEditor::film_content_change (ChangeType type, int property)
{
	if (type != CHANGE_TYPE_DONE) {
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
	set_general_sensitivity (film != 0);

	if (_film == film) {
		return;
	}

	_film = film;

	_content_panel->set_film (_film);
	_dcp_panel->set_film (_film);

	if (!_film) {
		FileChanged ("");
		return;
	}

	_film->Change.connect (bind (&FilmEditor::film_change, this, _1, _2));
	_film->ContentChange.connect (bind (&FilmEditor::film_content_change, this, _1, _3));

	if (_film->directory()) {
		FileChanged (_film->directory().get());
	} else {
		FileChanged ("");
	}

	if (!_film->content().empty()) {
		_content_panel->set_selection (_film->content().front ());
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
