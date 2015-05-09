/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <wx/wx.h>
#include "lib/film.h"
#include "lib/audio_analysis.h"

class AudioPlot;
class Film;

class AudioDialog : public wxDialog
{
public:
	AudioDialog (wxWindow *, boost::shared_ptr<Film> film);

	void set_content (boost::shared_ptr<AudioContent>);

private:
	void content_changed (int);
	void channel_clicked (wxCommandEvent &);
	void type_clicked (wxCommandEvent &);
	void smoothing_changed ();
	void try_to_load_analysis ();
	void analysis_finished ();
	void setup_peak_time ();

	boost::shared_ptr<AudioContent> _content;
	boost::shared_ptr<AudioAnalysis> _analysis;
	boost::weak_ptr<Film> _film;
	AudioPlot* _plot;
	wxStaticText* _peak_time;
	wxCheckBox* _channel_checkbox[MAX_DCP_AUDIO_CHANNELS];
	wxCheckBox* _type_checkbox[AudioPoint::COUNT];
	wxSlider* _smoothing;
	boost::signals2::scoped_connection _content_changed_connection;
	boost::signals2::scoped_connection _analysis_finished_connection;
};
