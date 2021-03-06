/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

#include "content_menu.h"
#include "lib/types.h"
#include "lib/film.h"
#include <wx/splitter.h>
#include <boost/shared_ptr.hpp>
#include <list>

class wxNotebook;
class wxPanel;
class wxSizer;
class wxListCtrl;
class wxListEvent;
class wxSplitterWindow;
class TimelineDialog;
class FilmEditor;
class ContentSubPanel;
class TextPanel;
class VideoPanel;
class AudioPanel;
class TimingPanel;
class Film;
class FilmViewer;

class ContentPanel : public boost::noncopyable
{
public:
	ContentPanel (wxNotebook *, boost::shared_ptr<Film>, boost::weak_ptr<FilmViewer> viewer);

	boost::shared_ptr<Film> film () const {
		return _film;
	}

	void set_film (boost::shared_ptr<Film>);
	void set_general_sensitivity (bool s);
	void set_selection (boost::weak_ptr<Content>);
	void set_selection (ContentList cl);

	void film_changed (Film::Property p);
	void film_content_changed (int p);

	wxWindow* window () const {
		return _splitter;
	}

	wxNotebook* notebook () const {
		return _notebook;
	}

	ContentList selected ();
	ContentList selected_video ();
	ContentList selected_audio ();
	ContentList selected_text ();
	FFmpegContentList selected_ffmpeg ();

	void add_file_clicked ();
	bool remove_clicked (bool hotkey);
	void timeline_clicked ();

	boost::weak_ptr<FilmViewer> film_viewer () const {
		return _film_viewer;
	}

	boost::signals2::signal<void (void)> SelectionChanged;

private:
	void item_selected ();
	void item_deselected ();
	void item_deselected_idle ();
	void check_selection ();
	void add_folder_clicked ();
	void add_dcp_clicked ();
	void earlier_clicked ();
	void later_clicked ();
	void right_click (wxListEvent &);
	void files_dropped (wxDropFilesEvent &);

	void setup ();
	void setup_sensitivity ();

	void add_files (std::list<boost::filesystem::path>);
	std::list<ContentSubPanel *> panels () const;

	wxSplitterWindow* _splitter;
	wxNotebook* _notebook;
	wxListCtrl* _content;
	wxButton* _add_file;
	wxButton* _add_folder;
	wxButton* _add_dcp;
	wxButton* _remove;
	wxButton* _earlier;
	wxButton* _later;
	wxButton* _timeline;
	VideoPanel* _video_panel;
	AudioPanel* _audio_panel;
	TextPanel* _text_panel[TEXT_COUNT];
	TimingPanel* _timing_panel;
	ContentMenu* _menu;
	TimelineDialog* _timeline_dialog;
	wxNotebook* _parent;
	ContentList _last_selected;
	wxWindow* _last_selected_tab;

	boost::shared_ptr<Film> _film;
	boost::weak_ptr<FilmViewer> _film_viewer;
	bool _generally_sensitive;
	bool _ignore_deselect;
};
