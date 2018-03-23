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
class AudioPanel;
class Film;
class FilmViewer;

class ContentPanel : public boost::noncopyable
{
public:
	ContentPanel (wxNotebook *, boost::shared_ptr<Film>, FilmViewer* viewer);

	boost::shared_ptr<Film> film () const {
		return _film;
	}

	void set_film (boost::shared_ptr<Film>);
	void set_general_sensitivity (bool s);
	void set_selection (boost::weak_ptr<Content>);
	void set_selection (ContentList cl);

	void film_changed (Film::Property p);
	void film_content_changed (int p);

	wxPanel* panel () const {
		return _panel;
	}

	wxNotebook* notebook () const {
		return _notebook;
	}

	ContentList selected ();
	ContentList selected_video ();
	ContentList selected_audio ();
	ContentList selected_subtitle ();
	FFmpegContentList selected_ffmpeg ();

	void add_file_clicked ();
	bool remove_clicked (bool hotkey);
	void timeline_clicked ();

	boost::signals2::signal<void (void)> SelectionChanged;

private:
	void selection_changed ();
	void add_folder_clicked ();
	void add_dcp_clicked ();
	void earlier_clicked ();
	void later_clicked ();
	void right_click (wxListEvent &);
	void files_dropped (wxDropFilesEvent &);

	void setup ();
	void setup_sensitivity ();

	void add_files (std::list<boost::filesystem::path>);

	wxPanel* _panel;
	wxSizer* _sizer;
	wxNotebook* _notebook;
	wxListCtrl* _content;
	wxButton* _add_file;
	wxButton* _add_folder;
	wxButton* _add_dcp;
	wxButton* _remove;
	wxButton* _earlier;
	wxButton* _later;
	wxButton* _timeline;
	ContentSubPanel* _video_panel;
	AudioPanel* _audio_panel;
	ContentSubPanel* _subtitle_panel;
	ContentSubPanel* _timing_panel;
	std::list<ContentSubPanel *> _panels;
	ContentMenu* _menu;
	TimelineDialog* _timeline_dialog;
	wxNotebook* _parent;
	ContentList _last_selected;

	boost::shared_ptr<Film> _film;
	FilmViewer* _film_viewer;
	bool _generally_sensitive;
};
