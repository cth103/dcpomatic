/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_CONTROLS_H
#define DCPOMATIC_CONTROLS_H

#include "lib/dcpomatic_time.h"
#include "lib/types.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class FilmViewer;
class Film;
class ClosedCaptionsDialog;
class Content;
class PlayerVideo;
class wxToggleButton;
class wxListCtrl;
class ContentView;

namespace dcp {
	class CPL;
}

class Controls : public wxPanel
{
public:
	Controls (
		wxWindow* parent,
		boost::shared_ptr<FilmViewer>,
		bool editor_controls = true
		);

	virtual void log (wxString) {}
	virtual void set_film (boost::shared_ptr<Film> film);
#ifdef DCPOMATIC_PLAYER_STRESS_TEST
	virtual void play () {};
	virtual void stop () {};
	void seek (int slider);
#endif
	boost::shared_ptr<Film> film () const;
	void back_frame ();
	void forward_frame ();

protected:
	virtual void started ();
	virtual void stopped ();
	virtual void setup_sensitivity ();
	virtual void config_changed (int property);

	wxSizer* _v_sizer;
	wxBoxSizer* _button_sizer;
	boost::shared_ptr<Film> _film;
	wxSlider* _slider;
	boost::shared_ptr<FilmViewer> _viewer;
	boost::optional<std::string> _active_job;

private:

	void update_position_label ();
	void update_position_slider ();
	void rewind_clicked (wxMouseEvent &);
	void back_clicked (wxKeyboardState& s);
	void forward_clicked (wxKeyboardState &);
	void slider_moved (bool page);
	void slider_released ();
	void frame_number_clicked ();
	void jump_to_selected_clicked ();
	void timecode_clicked ();
	void active_jobs_changed (boost::optional<std::string>);
	dcpomatic::DCPTime nudge_amount (wxKeyboardState& ev);
	void image_changed (boost::weak_ptr<PlayerVideo>);
	void outline_content_changed ();
	void eye_changed ();
	void update_position ();
	void film_change (ChangeType, Film::Property);

	typedef std::pair<boost::shared_ptr<dcp::CPL>, boost::filesystem::path> CPL;

	bool _slider_being_moved;

	wxCheckBox* _outline_content;
	wxChoice* _eye;
	wxCheckBox* _jump_to_selected;
	wxButton* _rewind_button;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;

	ClosedCaptionsDialog* _closed_captions_dialog;

	wxTimer _timer;

	boost::signals2::scoped_connection _film_change_connection;
	boost::signals2::scoped_connection _config_changed_connection;
};

#endif
