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


#include "content_menu.h"
#include "lib/enum_indexed_vector.h"
#include "lib/film.h"
#include "lib/text_type.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/splitter.h>
LIBDCP_ENABLE_WARNINGS
#include <list>


class AudioPanel;
class ContentListCtrl;
class ContentSubPanel;
class Film;
class FilmEditor;
class FilmViewer;
class LimitedContentPanelSplitter;
class TextPanel;
class TimelineDialog;
class TimingPanel;
class VideoPanel;
class wxListCtrl;
class wxListEvent;
class wxNotebook;
class wxPanel;
class wxSizer;
class wxSplitterWindow;


class ContentPanel
{
public:
	ContentPanel(wxNotebook *, std::shared_ptr<Film>, FilmViewer& viewer);

	ContentPanel (ContentPanel const&) = delete;
	ContentPanel& operator= (ContentPanel const&) = delete;

	std::shared_ptr<Film> film () const {
		return _film;
	}

	void set_film (std::shared_ptr<Film>);
	void set_general_sensitivity (bool s);
	void set_selection (std::weak_ptr<Content>);
	void set_selection (ContentList cl);
	void select_all ();

	void film_changed (Film::Property p);
	void film_content_changed (int p);

	void first_shown ();

	wxWindow* window () const;

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

	FilmViewer& film_viewer() const {
		return _film_viewer;
	}

	void add_files(std::vector<boost::filesystem::path> files);
	void add_dcp(boost::filesystem::path dcp);
	void add_folder(boost::filesystem::path folder);

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
	boost::optional<boost::filesystem::path> add_files_override_path() const;

	void setup ();
	void setup_sensitivity ();
	void set_selected_state(int item, bool state);

	std::list<ContentSubPanel *> panels () const;

	LimitedContentPanelSplitter* _splitter;
	wxPanel* _top_panel;
	wxNotebook* _notebook;
	ContentListCtrl* _content;
	wxButton* _add_file;
	wxButton* _add_folder;
	wxButton* _add_dcp;
	wxButton* _remove;
	wxButton* _earlier;
	wxButton* _later;
	wxButton* _timeline;
	VideoPanel* _video_panel = nullptr;
	AudioPanel* _audio_panel = nullptr;
	EnumIndexedVector<TextPanel*, TextType> _text_panel;
	TimingPanel* _timing_panel;
	ContentMenu* _menu;
	TimelineDialog* _timeline_dialog = nullptr;
	wxNotebook* _parent;
	wxWindow* _last_selected_tab = nullptr;

	std::shared_ptr<Film> _film;
	FilmViewer& _film_viewer;
	bool _generally_sensitive;
	bool _ignore_deselect;
	bool _no_check_selection;
};
