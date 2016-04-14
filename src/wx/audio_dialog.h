/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/audio_analysis.h"
#include "lib/playlist.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class AudioPlot;
class Film;

class AudioDialog : public wxDialog
{
public:
	AudioDialog (wxWindow *, boost::shared_ptr<Film> film, boost::shared_ptr<Content> content = boost::shared_ptr<Content> ());

	bool Show (bool show = true);

private:
	void content_changed (int);
	void channel_clicked (wxCommandEvent &);
	void type_clicked (wxCommandEvent &);
	void smoothing_changed ();
	void try_to_load_analysis ();
	void analysis_finished ();
	void setup_statistics ();

	boost::shared_ptr<AudioAnalysis> _analysis;
	boost::weak_ptr<Film> _film;
	boost::weak_ptr<AudioContent> _content;
	int _channels;
	boost::shared_ptr<const Playlist> _playlist;
	AudioPlot* _plot;
	wxStaticText* _sample_peak;
	wxStaticText* _true_peak;
	wxStaticText* _integrated_loudness;
	wxStaticText* _loudness_range;
	wxCheckBox* _channel_checkbox[MAX_DCP_AUDIO_CHANNELS];
	wxCheckBox* _type_checkbox[AudioPoint::COUNT];
	wxSlider* _smoothing;
	boost::signals2::scoped_connection _film_connection;
	boost::signals2::scoped_connection _analysis_finished_connection;
};
