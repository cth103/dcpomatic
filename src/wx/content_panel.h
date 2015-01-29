/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <list>
#include <boost/shared_ptr.hpp>
#include "lib/types.h"
#include "lib/film.h"
#include "content_menu.h"

class wxNotebook;
class wxPanel;
class wxSizer;
class wxListCtrl;
class wxListEvent;
class TimelineDialog;
class FilmEditor;
class ContentSubPanel;
class Film;

class ContentPanel : public boost::noncopyable
{
public:
	ContentPanel (wxNotebook *, boost::shared_ptr<Film>);

	boost::shared_ptr<Film> film () const {
		return _film;
	}

	void set_film (boost::shared_ptr<Film> f);
	void set_general_sensitivity (bool s);
	void set_selection (boost::weak_ptr<Content>);

	void film_changed (Film::Property p);
	void film_content_changed (int p);

	wxPanel* panel () const {
		return _panel;
	}

	wxNotebook* notebook () const {
		return _notebook;
	}

	ContentList selected ();
	VideoContentList selected_video ();
	AudioContentList selected_audio ();
	SubtitleContentList selected_subtitle ();
	FFmpegContentList selected_ffmpeg ();

	void add_file_clicked ();
	
private:	
	void selection_changed ();
	void add_folder_clicked ();
	void remove_clicked ();
	void earlier_clicked ();
	void later_clicked ();
	void right_click (wxListEvent &);
	void files_dropped (wxDropFilesEvent &);
	void timeline_clicked ();

	void setup ();
	void setup_sensitivity ();

	wxPanel* _panel;
	wxSizer* _sizer;
	wxNotebook* _notebook;
	wxListCtrl* _content;
	wxButton* _add_file;
	wxButton* _add_folder;
	wxButton* _remove;
	wxButton* _earlier;
	wxButton* _later;
	wxButton* _timeline;
	ContentSubPanel* _video_panel;
	ContentSubPanel* _audio_panel;
	ContentSubPanel* _subtitle_panel;
	ContentSubPanel* _timing_panel;
	std::list<ContentSubPanel *> _panels;
	ContentMenu* _menu;
	TimelineDialog* _timeline_dialog;

	boost::shared_ptr<Film> _film;
	bool _generally_sensitive;
};
