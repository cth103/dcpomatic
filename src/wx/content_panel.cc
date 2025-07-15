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


#include "audio_panel.h"
#include "content_panel.h"
#include "content_timeline_dialog.h"
#include "dcpomatic_button.h"
#include "dir_dialog.h"
#include "file_dialog.h"
#include "film_viewer.h"
#include "image_sequence_dialog.h"
#include "text_panel.h"
#include "timing_panel.h"
#include "video_panel.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/audio_content.h"
#include "lib/case_insensitive_sorter.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/dcpomatic_log.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/image_content.h"
#include "lib/log.h"
#include "lib/playlist.h"
#include "lib/string_text_file.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include <dcp/filesystem.h>
#include <dcp/scope_guard.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


class LimitedContentPanelSplitter : public wxSplitterWindow
{
public:
	LimitedContentPanelSplitter(wxWindow* parent)
		: wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_NOBORDER | wxSP_3DSASH | wxSP_LIVE_UPDATE)
	{
		/* This value doesn't really mean much but we just want to stop double-click on the
		   divider from shrinking the bottom panel (#1601).
		   */
		SetMinimumPaneSize(64);

		Bind(wxEVT_SIZE, boost::bind(&LimitedContentPanelSplitter::sized, this, _1));
	}

	bool OnSashPositionChange(int new_position) override
	{
		/* Try to stop the top bit of the splitter getting so small that buttons disappear */
		auto const ok = new_position > 220;
		if (ok) {
			Config::instance()->set_main_content_divider_sash_position(new_position);
		}
		return ok;
	}

	void first_shown(wxWindow* top, wxWindow* bottom)
	{
		int const sn = wxDisplay::GetFromWindow(this);
		/* Fallback for when GetFromWindow fails for reasons that aren't clear */
		int pos = -600;
		if (sn >= 0) {
			wxRect const screen = wxDisplay(sn).GetClientArea();
			/* This is a hack to try and make the content notebook a sensible size; large on big displays but small
			   enough on small displays to leave space for the content area.
			   */
			pos = screen.height > 800 ? -600 : -_top_panel_minimum_size;
		}
		SplitHorizontally(top, bottom, Config::instance()->main_content_divider_sash_position().get_value_or(pos));
		_first_shown = true;
	}

private:
	void sized(wxSizeEvent& ev)
	{
		auto const height = GetSize().GetHeight();
		if (_first_shown && (!_last_height || *_last_height != height) && height > _top_panel_minimum_size && GetSashPosition() < _top_panel_minimum_size) {
			/* The window is now fairly big but the top panel is small; this happens when the DCP-o-matic window
			 * is shrunk and then made larger again.  Try to set a sensible top panel size in this case (#1839).
			 */
			SetSashPosition(Config::instance()->main_content_divider_sash_position().get_value_or(_top_panel_minimum_size));
		}

		ev.Skip();
		_last_height = height;
	}

	bool _first_shown = false;
	int const _top_panel_minimum_size = 350;
	boost::optional<int> _last_height;
};


class ContentDropTarget : public wxFileDropTarget
{
public:
	ContentDropTarget(ContentPanel* owner)
		: _panel(owner)
	{}

	bool OnDropFiles(wxCoord, wxCoord, wxArrayString const& filenames) override
	{
		vector<boost::filesystem::path> files;
		vector<boost::filesystem::path> dcps;
		vector<boost::filesystem::path> folders;
		for (size_t i = 0; i < filenames.GetCount(); ++i) {
			auto path = boost::filesystem::path(wx_to_std(filenames[i]));
			if (dcp::filesystem::is_regular_file(path)) {
				files.push_back(path);
			} else if (dcp::filesystem::is_directory(path)) {
				if (contains_assetmap(path)) {
					dcps.push_back(path);
				} else {
					folders.push_back(path);
				}
			}
		}

		if (!filenames.empty()) {
			_panel->add_files(files);
		}

		for (auto dcp: dcps) {
			_panel->add_dcp(dcp);
		}

		for (auto dir: folders) {
			_panel->add_folder(dir);
		}

		return true;
	};

private:
	ContentPanel* _panel;
};


/** A wxListCtrl that can middle-ellipsize its text */
class ContentListCtrl : public wxListCtrl
{
public:
	ContentListCtrl(wxWindow* parent)
		: wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxSize(320, 160), wxLC_REPORT | wxLC_NO_HEADER | wxLC_VIRTUAL)
	{
		_red.SetTextColour(*wxRED);
	}

	struct Item
	{
		wxString text;
		weak_ptr<Content> content;
		bool error;
	};

	void set(vector<Item> const& items)
	{
		_items = items;
		SetItemCount(items.size());
	}

	wxString OnGetItemText(long item, long) const override
	{
		if (out_of_range(item)) {
			/* wxWidgets sometimes asks for things that are already gone */
			return {};
		}
		wxClientDC dc(const_cast<wxWindow*>(static_cast<wxWindow const*>(this)));
		return wxControl::Ellipsize(_items[item].text, dc, wxELLIPSIZE_MIDDLE, GetSize().GetWidth());
	}

	wxListItemAttr* OnGetItemAttr(long item) const override
	{
		if (out_of_range(item)) {
			return nullptr;
		}
		return _items[item].error ? const_cast<wxListItemAttr*>(&_red) : nullptr;
	}

	weak_ptr<Content> content_at_index(long index)
	{
		if (out_of_range(index)) {
			return {};
		}
		return _items[index].content;
	}

private:
	bool out_of_range(long index) const {
		return index < 0 || index >= static_cast<long>(_items.size());
	}

	std::vector<Item> _items;
	wxListItemAttr _red;
};


ContentPanel::ContentPanel(wxNotebook* n, shared_ptr<Film> film, FilmViewer& viewer)
	: _parent(n)
	, _film(film)
	, _film_viewer(viewer)
	, _generally_sensitive(true)
	, _ignore_deselect(false)
	, _no_check_selection(false)
{
	_splitter = new LimitedContentPanelSplitter(n);
	_top_panel = new wxPanel(_splitter);

	_menu = new ContentMenu(_splitter, _film_viewer);

	{
		auto s = new wxBoxSizer(wxHORIZONTAL);

		_content = new ContentListCtrl(_top_panel);
		_content->DragAcceptFiles(true);
		s->Add(_content, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);

		_content->InsertColumn(0, wxString{});
		_content->SetColumnWidth(0, 2048);

		auto b = new wxBoxSizer(wxVERTICAL);

		_add_file = new Button(_top_panel, _("Add file(s)..."));
		_add_file->SetToolTip(_("Add video, image, sound or subtitle files to the film (Ctrl+A)."));
		b->Add(_add_file, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_add_folder = new Button(_top_panel, _("Add folder..."));
		_add_folder->SetToolTip(_("Add a folder of image files (which will be used as a moving image sequence) or a folder of sound files."));
		b->Add(_add_folder, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_add_dcp = new Button(_top_panel, _("Add DCP..."));
		_add_dcp->SetToolTip(_("Add a DCP."));
		b->Add(_add_dcp, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_remove = new Button(_top_panel, _("Remove"));
		_remove->SetToolTip(_("Remove the selected piece of content from the film (Delete)."));
		b->Add(_remove, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_earlier = new Button(_top_panel, _("Earlier"));
		_earlier->SetToolTip(_("Move the selected piece of content earlier in the film."));
		b->Add(_earlier, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_later = new Button(_top_panel, _("Later"));
		_later->SetToolTip(_("Move the selected piece of content later in the film."));
		b->Add(_later, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_timeline = new Button(_top_panel, _("Timeline..."));
		_timeline->SetToolTip(_("Open the timeline for the film (Ctrl+T)."));
		b->Add(_timeline, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		s->Add(b, 0, wxALL, 4);
		_top_panel->SetSizer(s);
	}

	_notebook = new wxNotebook(_splitter, wxID_ANY);

	_timing_panel = new TimingPanel(this, _film_viewer);
	_notebook->AddPage(_timing_panel, _("Timing"), false);
	_timing_panel->create();

	_content->Bind(wxEVT_LIST_ITEM_SELECTED, boost::bind(&ContentPanel::item_selected, this));
	_content->Bind(wxEVT_LIST_ITEM_DESELECTED, boost::bind(&ContentPanel::item_deselected, this));
	_content->Bind(wxEVT_LIST_ITEM_FOCUSED, boost::bind(&ContentPanel::item_focused, this));
	_content->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, boost::bind(&ContentPanel::right_click, this, _1));
	_content->Bind(wxEVT_DROP_FILES, boost::bind(&ContentPanel::files_dropped, this, _1));
	_add_file->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::add_file_clicked, this));
	_add_folder->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::add_folder_clicked, this));
	_add_dcp->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::add_dcp_clicked, this));
	_remove->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::remove_clicked, this, false));
	_earlier->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::earlier_clicked, this));
	_later->Bind(wxEVT_BUTTON, boost::bind(&ContentPanel::later_clicked, this));
	_timeline->Bind	(wxEVT_BUTTON, boost::bind(&ContentPanel::timeline_clicked, this));

	_content->SetDropTarget(new ContentDropTarget(this));
}


void
ContentPanel::first_shown()
{
	_splitter->first_shown(_top_panel, _notebook);
}


ContentList
ContentPanel::selected()
{
	ContentList sel;
	long int s = -1;
	while (true) {
		s = _content->GetNextItem(s, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			break;
		}

		auto weak = _content->content_at_index(s);
		if (auto content = weak.lock()) {
			sel.push_back(content);
		}
	}

	return sel;
}


ContentList
ContentPanel::selected_video()
{
	ContentList vc;

	for (auto i: selected()) {
		if (i->video) {
			vc.push_back(i);
		}
	}

	return vc;
}


ContentList
ContentPanel::selected_audio()
{
	ContentList ac;

	for (auto i: selected()) {
		if (i->audio) {
			ac.push_back(i);
		}
	}

	return ac;
}


ContentList
ContentPanel::selected_text()
{
	ContentList sc;

	for (auto i: selected()) {
		if (!i->text.empty()) {
			sc.push_back(i);
		}
	}

	return sc;
}


FFmpegContentList
ContentPanel::selected_ffmpeg()
{
	FFmpegContentList sc;

	for (auto i: selected()) {
		if (auto t = dynamic_pointer_cast<FFmpegContent>(i)) {
			sc.push_back(t);
		}
	}

	return sc;
}


void
ContentPanel::film_changed(FilmProperty p)
{
	switch (p) {
	case FilmProperty::CONTENT:
	case FilmProperty::CONTENT_ORDER:
		setup();
		break;
	default:
		break;
	}

	for (auto i: panels()) {
		i->film_changed(p);
	}
}


void
ContentPanel::item_deselected()
{
	/* Maybe this is just a re-click on the same item; if not, _ignore_deselect will stay
	   false and item_deselected_foo will handle the deselection.
	*/
	_ignore_deselect = false;
	signal_manager->when_idle(boost::bind(&ContentPanel::item_deselected_idle, this));
}


void
ContentPanel::item_deselected_idle()
{
	if (!_ignore_deselect) {
		check_selection();
	}
}


void
ContentPanel::item_selected()
{
	_ignore_deselect = true;
	check_selection();
}


void
ContentPanel::item_focused()
{
	signal_manager->when_idle(boost::bind(&ContentPanel::check_selection, this));
}


void
ContentPanel::check_selection()
{
	if (_no_check_selection) {
		return;
	}

	setup_sensitivity();

	for (auto i: panels()) {
		i->content_selection_changed();
	}

	optional<DCPTime> go_to;
	for (auto content: selected()) {
		if (paths_exist(content->paths())) {
			auto position = content->position();
			if (auto text_content = dynamic_pointer_cast<StringTextFileContent>(content)) {
				/* Rather special case; if we select a text subtitle file jump to its
				   first subtitle.
				*/
				StringTextFile ts(text_content);
				if (auto first = ts.first()) {
					position += DCPTime(first.get(), _film->active_frame_rate_change(content->position()));
				}
			} else if (auto dcp_content = dynamic_pointer_cast<DCPSubtitleContent>(content)) {
				/* Do the same for DCP subtitles */
				DCPSubtitleDecoder ts(_film, dcp_content);
				if (auto first = ts.first()) {
					position += DCPTime(first.get(), _film->active_frame_rate_change(content->position()));
				}
			}
			if (!go_to || position < go_to.get()) {
				go_to = position;
			}
		}
	}

	if (go_to && Config::instance()->jump_to_selected() && signal_manager) {
		signal_manager->when_idle(boost::bind(&FilmViewer::seek, &_film_viewer, go_to.get().ceil(_film->video_frame_rate()), true));
	}

	if (_timeline_dialog) {
		_timeline_dialog->set_selection(selected());
	}

	/* Make required tabs visible */

	if (_notebook->GetPageCount() > 1) {
		/* There's more than one tab in the notebook so the current selection could be meaningful
		   to the user; store it so that we can try to restore it later.
		*/
		_last_selected_tab = 0;
		if (_notebook->GetSelection() != wxNOT_FOUND) {
			_last_selected_tab = _notebook->GetPage(_notebook->GetSelection());
		}
	}

	bool have_video = false;
	bool have_audio = false;
	EnumIndexedVector<bool, TextType> have_text;
	for (auto i: selected()) {
		if (i->video) {
			have_video = true;
		}
		if (i->audio) {
			have_audio = true;
		}
		for (auto j: i->text) {
			have_text[static_cast<int>(j->original_type())] = true;
		}
	}

	int off = 0;

	if (have_video && !_video_panel) {
		_video_panel = new VideoPanel(this);
		_notebook->InsertPage(off, _video_panel, _video_panel->name());
		_video_panel->create();
	} else if (!have_video && _video_panel) {
		_notebook->DeletePage(off);
		_video_panel = nullptr;
	}

	if (have_video) {
		++off;
	}

	if (have_audio && !_audio_panel) {
		_audio_panel = new AudioPanel(this);
		_notebook->InsertPage(off, _audio_panel, _audio_panel->name());
		_audio_panel->create();
	} else if (!have_audio && _audio_panel) {
		_notebook->DeletePage(off);
		_audio_panel = nullptr;
	}

	if (have_audio) {
		++off;
	}

	for (int i = 0; i < static_cast<int>(TextType::COUNT); ++i) {
		if (have_text[i] && !_text_panel[i]) {
			_text_panel[i] = new TextPanel(this, static_cast<TextType>(i));
			_notebook->InsertPage(off, _text_panel[i], _text_panel[i]->name());
			_text_panel[i]->create();
		} else if (!have_text[i] && _text_panel[i]) {
			_notebook->DeletePage(off);
			_text_panel[i] = nullptr;
		}
		if (have_text[i]) {
			++off;
		}
	}

	/* Set up the tab selection */

	auto done = false;
	for (size_t i = 0; i < _notebook->GetPageCount(); ++i) {
		if (_notebook->GetPage(i) == _last_selected_tab) {
			_notebook->SetSelection(i);
			done = true;
		}
	}

	if (!done && _notebook->GetPageCount() > 0) {
		_notebook->SetSelection(0);
	}

	setup_sensitivity();
	SelectionChanged();
}


void
ContentPanel::add_file_clicked()
{
	/* This method is also called when Ctrl-A is pressed, so check that our notebook page
	   is visible.
	*/
	if (_parent->GetCurrentPage() != _splitter || !_film) {
		return;
	}

	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	FileDialog dialog(
		_splitter,
		_("Choose a file or files"),
		char_to_wx("All files|*.*|Subtitle files|*.srt;*.xml|Audio files|*.wav;*.w64;*.flac;*.aif;*.aiff"),
		wxFD_MULTIPLE | wxFD_CHANGE_DIR,
		"AddFilesPath",
		{},
		dcpomatic::film::add_files_override_path(_film)
		);

	if (dialog.show()) {
		add_files(dialog.paths());
	}
}


void
ContentPanel::add_folder_clicked()
{
	DirDialog dialog(_splitter, _("Choose a folder"), wxDD_DIR_MUST_EXIST, "AddFilesPath", dcpomatic::film::add_files_override_path(_film));
	if (dialog.show()) {
		add_folder(dialog.path());
	}
}


void
ContentPanel::add_folder(boost::filesystem::path folder)
{
	vector<shared_ptr<Content>> content;

	try {
		content = content_factory(folder);
	} catch (exception& e) {
		error_dialog(_parent, std_to_wx(e.what()));
		return;
	}

	if (content.empty()) {
		error_dialog(_parent, _("No content found in this folder."));
		return;
	}

	for (auto i: content) {
		if (auto ic = dynamic_pointer_cast<ImageContent>(i)) {
			ImageSequenceDialog dialog(_splitter);

			if (dialog.ShowModal() != wxID_OK) {
				return;
			}
			ic->set_video_frame_rate(_film, dialog.frame_rate());
		}

	}

	_film->examine_and_add_content(content);
}


void
ContentPanel::add_dcp_clicked()
{
	DirDialog dialog(_splitter, _("Choose a DCP folder"), wxDD_DIR_MUST_EXIST, "AddFilesPath", dcpomatic::film::add_files_override_path(_film));
	if (dialog.show()) {
		add_dcp(dialog.path());
	}
}


void
ContentPanel::add_dcp(boost::filesystem::path dcp)
{
	try {
		_film->examine_and_add_content({make_shared<DCPContent>(dcp)});
	} catch (ProjectFolderError &) {
		error_dialog(
			_parent,
			wxString::Format(
				_(
					"This looks like a %s project folder, which cannot be added to a different project.  "
					"Choose the DCP folder inside the %s project folder if that's what you want to import."
				 ),
				variant::wx::dcpomatic(),
				variant::wx::dcpomatic()
				)
			);
	} catch (exception& e) {
		error_dialog(_parent, std_to_wx(e.what()));
	}
}


/** @return true if this remove "click" should be ignored */
bool
ContentPanel::remove_clicked(bool hotkey)
{
	/* If the method was called because Delete was pressed check that our notebook page
	   is visible and that the content list is focused.
	*/
	if (hotkey && (_parent->GetCurrentPage() != _splitter || !_content->HasFocus())) {
		return true;
	}

	for (auto i: selected()) {
		_film->remove_content(i);
	}

	check_selection();
	return false;
}


void
ContentPanel::timeline_clicked()
{
	if (!_film || _film->content().empty()) {
		return;
	}

	_timeline_dialog.reset(this, _film, _film_viewer);
	_timeline_dialog->set_selection(selected());
	_timeline_dialog->Show();
}


void
ContentPanel::right_click(wxListEvent& ev)
{
	_menu->popup(_film, selected(), TimelineContentViewList(), ev.GetPoint());
}


/** Set up broad sensitivity based on the type of content that is selected */
void
ContentPanel::setup_sensitivity ()
{
	_add_file->Enable(_generally_sensitive);
	_add_folder->Enable(_generally_sensitive);
	_add_dcp->Enable(_generally_sensitive);

	auto selection = selected();
	auto video_selection = selected_video ();
	auto audio_selection = selected_audio();

	_remove->Enable  (_generally_sensitive && !selection.empty());
	_earlier->Enable (_generally_sensitive && selection.size() == 1);
	_later->Enable   (_generally_sensitive && selection.size() == 1);
	_timeline->Enable(_generally_sensitive && _film && !_film->content().empty());

	if (_video_panel) {
		_video_panel->Enable(_generally_sensitive && video_selection.size() > 0);
	}
	if (_audio_panel) {
		_audio_panel->Enable(_generally_sensitive && audio_selection.size() > 0);
	}
	for (auto text: _text_panel) {
		if (text) {
			text->Enable(_generally_sensitive && selection.size() == 1 && !selection.front()->text.empty());
		}
	}
	_timing_panel->Enable(_generally_sensitive);
}


void
ContentPanel::set_film(shared_ptr<Film> film)
{
	if (_audio_panel) {
		_audio_panel->set_film(film);
	}

	_film = film;

	film_changed(FilmProperty::CONTENT);
	film_changed(FilmProperty::AUDIO_CHANNELS);

	if (_film) {
		check_selection();
	}

	setup_sensitivity();
}


void
ContentPanel::set_general_sensitivity(bool s)
{
	_generally_sensitive = s;
	setup_sensitivity();
}


void
ContentPanel::earlier_clicked()
{
	auto sel = selected();
	if (sel.size() == 1) {
		_film->move_content_earlier(sel.front());
		check_selection();
	}
}


void
ContentPanel::later_clicked()
{
	auto sel = selected();
	if (sel.size() == 1) {
		_film->move_content_later(sel.front());
		check_selection();
	}
}


void
ContentPanel::set_selection(weak_ptr<Content> wc)
{
	auto content = _film->content();
	for (size_t i = 0; i < content.size(); ++i) {
		set_selected_state(i, content[i] == wc.lock());
	}
}


void
ContentPanel::set_selection(ContentList cl)
{
	{
		_no_check_selection = true;
		dcp::ScopeGuard sg = [this]() { _no_check_selection = false; };

		auto content = _film->content();
		for (size_t i = 0; i < content.size(); ++i) {
			set_selected_state(i, find(cl.begin(), cl.end(), content[i]) != cl.end());
		}
	}

	check_selection();
}


void
ContentPanel::select_all()
{
	set_selection(_film->content());
}


void
ContentPanel::film_content_changed(int property)
{
	if (
		property == ContentProperty::PATH ||
		property == DCPContentProperty::NEEDS_ASSETS ||
		property == DCPContentProperty::NEEDS_KDM ||
		property == DCPContentProperty::NAME
		) {

		setup();
	}

	for (auto i: panels()) {
		i->film_content_changed(property);
	}
}


void
ContentPanel::setup()
{
	if (!_film) {
		_content->DeleteAllItems();
		setup_sensitivity();
		return;
	}

	auto content = _film->content();
	auto selection = selected();

	vector<ContentListCtrl::Item> items;

	for (auto i: content) {
		bool const valid = paths_exist(i->paths()) && paths_exist(i->font_paths());

		auto dcp = dynamic_pointer_cast<DCPContent>(i);
		bool const needs_kdm = dcp && dcp->needs_kdm();
		bool const needs_assets = dcp && dcp->needs_assets();

		auto s = std_to_wx(i->summary());

		if (!valid) {
			s = _("MISSING: ") + s;
		}

		if (needs_kdm) {
			s = _("NEEDS KDM: ") + s;
		}

		if (needs_assets) {
			s = _("NEEDS OV: ") + s;
		}

		items.push_back({s, i, !valid || needs_kdm || needs_assets});
	}

	_content->set(items);

	if (selection.empty() && !content.empty()) {
		set_selected_state(0, true);
	} else {
		set_selection(selection);
	}

	setup_sensitivity();
}


void
ContentPanel::files_dropped(wxDropFilesEvent& event)
{
	if (!_film) {
		return;
	}

	auto paths = event.GetFiles();
	vector<boost::filesystem::path> path_list;
	for (int i = 0; i < event.GetNumberOfFiles(); i++) {
		path_list.push_back(wx_to_std(paths[i]));
	}

	add_files(path_list);
}


void
ContentPanel::add_files(vector<boost::filesystem::path> paths)
{
	if (!_film) {
		return;
	}

	/* It has been reported that the paths returned from e.g. wxFileDialog are not always sorted;
	   I can't reproduce that, but sort them anyway.  Don't use ImageFilenameSorter as a normal
	   alphabetical sort is expected here.
	*/

	std::sort(paths.begin(), paths.end(), CaseInsensitiveSorter());

	/* XXX: check for lots of files here and do something */

	vector<shared_ptr<Content>> content;
	try {
		for (auto i: paths) {
			for (auto j: content_factory(i)) {
				content.push_back(j);
			}
		}
		_film->examine_and_add_content(content);
	} catch (exception& e) {
		error_dialog(_parent, std_to_wx(e.what()));
	}
}


list<ContentSubPanel*>
ContentPanel::panels() const
{
	list<ContentSubPanel*> p;
	if (_video_panel) {
		p.push_back(_video_panel);
	}
	if (_audio_panel) {
		p.push_back(_audio_panel);
	}
	for (auto text: _text_panel) {
		if (text) {
			p.push_back(text);
		}
	}
	p.push_back(_timing_panel);
	return p;
}


void
ContentPanel::set_selected_state(int item, bool state)
{
	_content->SetItemState(item, state ? wxLIST_STATE_SELECTED : 0, wxLIST_STATE_SELECTED);
	_content->SetItemState(item, state ? wxLIST_STATE_FOCUSED : 0, wxLIST_STATE_FOCUSED);
}


wxWindow*
ContentPanel::window() const
{
	return _splitter;
}
