/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/about_dialog.h"
#include "wx/content_view.h"
#include "wx/dcpomatic_button.h"
#include "wx/playlist_editor_config_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/spl.h"
#include "lib/spl_entry.h"
#include "lib/util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using std::exception;
using std::make_pair;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using std::dynamic_pointer_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static
void
save_playlist(shared_ptr<const SPL> playlist)
{
	if (auto dir = Config::instance()->player_playlist_directory()) {
		playlist->write(*dir / (playlist->id() + ".xml"));
	}
}


class ContentDialog : public wxDialog, public ContentStore
{
public:
	ContentDialog (wxWindow* parent)
		: wxDialog (parent, wxID_ANY, _("Add content"), wxDefaultPosition, wxSize(800, 640))
		, _content_view (new ContentView(this))
	{
		_content_view->update ();

		auto overall_sizer = new wxBoxSizer (wxVERTICAL);
		SetSizer (overall_sizer);

		overall_sizer->Add (_content_view, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
		if (buttons) {
			overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
		}

		overall_sizer->Layout ();

		_config_changed_connection = Config::instance()->Changed.connect(boost::bind(&ContentView::update, _content_view));
	}

	shared_ptr<Content> selected () const
	{
		return _content_view->selected ();
	}

	shared_ptr<Content> get (string digest) const override
	{
		return _content_view->get (digest);
	}

private:
	ContentView* _content_view;
	boost::signals2::scoped_connection _config_changed_connection;
};



class PlaylistList
{
public:
	PlaylistList (wxPanel* parent, ContentStore* content_store)
		: _sizer (new wxBoxSizer(wxVERTICAL))
		, _content_store (content_store)
		, _parent(parent)
	{
		auto label = new wxStaticText (parent, wxID_ANY, wxEmptyString);
		label->SetLabelMarkup (_("<b>Playlists</b>"));
		_sizer->Add (label, 0, wxTOP | wxLEFT, DCPOMATIC_SIZER_GAP * 2);

		_list = new wxListCtrl (
			parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL
			);

		_list->AppendColumn (_("Name"), wxLIST_FORMAT_LEFT, 840);
		_list->AppendColumn (_("Length"), wxLIST_FORMAT_LEFT, 100);

		auto button_sizer = new wxBoxSizer (wxVERTICAL);
		_new = new Button (parent, _("New"));
		button_sizer->Add (_new, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		_delete = new Button (parent, _("Delete"));
		button_sizer->Add (_delete, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

		auto list = new wxBoxSizer (wxHORIZONTAL);
		list->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
		list->Add (button_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);

		_sizer->Add (list);

		load_playlists ();

		_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, bind(&PlaylistList::selection_changed, this));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, bind(&PlaylistList::selection_changed, this));
		_new->Bind (wxEVT_BUTTON, bind(&PlaylistList::new_playlist, this));
		_delete->Bind (wxEVT_BUTTON, bind(&PlaylistList::delete_playlist, this));

		setup_sensitivity();
	}

	wxSizer* sizer ()
	{
		return _sizer;
	}

	shared_ptr<SignalSPL> first_playlist () const
	{
		if (_playlists.empty()) {
			return {};
		}

		return _playlists.front ();
	}

	boost::signals2::signal<void (shared_ptr<SignalSPL>)> Edit;

private:
	void setup_sensitivity()
	{
		_delete->Enable(static_cast<bool>(selected()));
	}

	void add_playlist_to_view (shared_ptr<const SignalSPL> playlist)
	{
		wxListItem item;
		item.SetId (_list->GetItemCount());
		long const N = _list->InsertItem (item);
		_list->SetItem (N, 0, std_to_wx(playlist->name()));
	}

	void add_playlist_to_model (shared_ptr<SignalSPL> playlist)
	{
		_playlists.push_back (playlist);
		playlist->Changed.connect(bind(&PlaylistList::changed, this, weak_ptr<SignalSPL>(playlist), _1));
	}

	void changed(weak_ptr<SignalSPL> wp, SignalSPL::Change change)
	{
		auto playlist = wp.lock ();
		if (!playlist) {
			return;
		}

		switch (change) {
		case SignalSPL::Change::NAME:
		{
			int N = 0;
			for (auto i: _playlists) {
				if (i == playlist) {
					_list->SetItem (N, 0, std_to_wx(i->name()));
				}
				++N;
			}
			break;
		}
		case SignalSPL::Change::CONTENT:
			save_playlist(playlist);
			break;
		}
	}

	void load_playlists ()
	{
		auto path = Config::instance()->player_playlist_directory();
		if (!path) {
			return;
		}

		_list->DeleteAllItems ();
		_playlists.clear ();
		for (auto i: boost::filesystem::directory_iterator(*path)) {
			auto spl = make_shared<SignalSPL>();
			try {
				spl->read (i, _content_store);
				add_playlist_to_model (spl);
			} catch (...) {}
		}

		for (auto i: _playlists) {
			add_playlist_to_view (i);
		}
	}

	void new_playlist ()
	{
		auto dir = Config::instance()->player_playlist_directory();
		if (!dir) {
			error_dialog(_parent, _("No playlist folder is specified in preferences.  Please set one and then try again."));
			return;
		}

		shared_ptr<SignalSPL> spl (new SignalSPL(wx_to_std(_("New Playlist"))));
		add_playlist_to_model (spl);
		add_playlist_to_view (spl);
		_list->SetItemState (_list->GetItemCount() - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}

	boost::optional<int> selected() const
	{
		long int selected = _list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected < 0 || selected >= int(_playlists.size())) {
			return {};
		}

		return selected;
	}

	void delete_playlist ()
	{
		auto index = selected();
		if (!index) {
			return;
		}

		auto dir = Config::instance()->player_playlist_directory();
		if (!dir) {
			return;
		}

		boost::filesystem::remove(*dir / (_playlists[*index]->id() + ".xml"));
		_list->DeleteItem(*index);
		_playlists.erase(_playlists.begin() + *index);

		Edit(shared_ptr<SignalSPL>());
	}

	void selection_changed ()
	{
		long int selected = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected < 0 || selected >= int(_playlists.size())) {
			Edit (shared_ptr<SignalSPL>());
		} else {
			Edit (_playlists[selected]);
		}

		setup_sensitivity();
	}

	wxBoxSizer* _sizer;
	wxListCtrl* _list;
	wxButton* _new;
	wxButton* _delete;
	vector<shared_ptr<SignalSPL>> _playlists;
	ContentStore* _content_store;
	wxWindow* _parent;
};


class PlaylistContent
{
public:
	PlaylistContent (wxPanel* parent, ContentDialog* content_dialog)
		: _content_dialog (content_dialog)
		, _sizer (new wxBoxSizer(wxVERTICAL))
	{
		auto title = new wxBoxSizer (wxHORIZONTAL);
		auto label = new wxStaticText (parent, wxID_ANY, wxEmptyString);
		label->SetLabelMarkup (_("<b>Playlist:</b>"));
		title->Add (label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_GAP);
		_name = new wxTextCtrl (parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(400, -1));
		title->Add (_name, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		_save_name = new Button(parent, _("Save"));
		title->Add(_save_name);
		_sizer->Add (title, 0, wxTOP | wxLEFT, DCPOMATIC_SIZER_GAP * 2);

		auto list = new wxBoxSizer (wxHORIZONTAL);

		_list = new wxListCtrl (
			parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL
			);

		_list->AppendColumn (_("Name"), wxLIST_FORMAT_LEFT, 400);
		_list->AppendColumn (_("CPL"), wxLIST_FORMAT_LEFT, 350);
		_list->AppendColumn (_("Type"), wxLIST_FORMAT_LEFT, 100);
		_list->AppendColumn (_("Encrypted"), wxLIST_FORMAT_CENTRE, 90);

		auto images = new wxImageList (16, 16);
		wxIcon tick_icon;
		wxIcon no_tick_icon;
		tick_icon.LoadFile (bitmap_path("tick.png"), wxBITMAP_TYPE_PNG);
		no_tick_icon.LoadFile (bitmap_path("no_tick.png"), wxBITMAP_TYPE_PNG);
		images->Add (tick_icon);
		images->Add (no_tick_icon);

		_list->SetImageList (images, wxIMAGE_LIST_SMALL);

		list->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

		auto button_sizer = new wxBoxSizer (wxVERTICAL);
		_up = new Button (parent, _("Up"));
		_down = new Button (parent, _("Down"));
		_add = new Button (parent, _("Add"));
		_remove = new Button (parent, _("Remove"));
		button_sizer->Add (_up, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_down, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_add, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_remove, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

		list->Add (button_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);

		_sizer->Add (list);

		_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, bind(&PlaylistContent::setup_sensitivity, this));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, bind(&PlaylistContent::setup_sensitivity, this));
		_name->Bind (wxEVT_TEXT, bind(&PlaylistContent::name_changed, this));
		_save_name->bind(&PlaylistContent::save_name_clicked, this);
		_up->Bind (wxEVT_BUTTON, bind(&PlaylistContent::up_clicked, this));
		_down->Bind (wxEVT_BUTTON, bind(&PlaylistContent::down_clicked, this));
		_add->Bind (wxEVT_BUTTON, bind(&PlaylistContent::add_clicked, this));
		_remove->Bind (wxEVT_BUTTON, bind(&PlaylistContent::remove_clicked, this));

		setup_sensitivity();
	}

	wxSizer* sizer ()
	{
		return _sizer;
	}

	void set (shared_ptr<SignalSPL> playlist)
	{
		_playlist = playlist;
		_list->DeleteAllItems ();
		if (_playlist) {
			for (auto i: _playlist->get()) {
				add (i);
			}
			_name->SetValue (std_to_wx(_playlist->name()));
		} else {
			_name->SetValue (wxT(""));
		}
		setup_sensitivity ();
	}

	shared_ptr<SignalSPL> playlist () const
	{
		return _playlist;
	}


private:
	void save_name_clicked()
	{
		if (_playlist) {
			_playlist->set_name(wx_to_std(_name->GetValue()));
			save_playlist(_playlist);
		}
		setup_sensitivity();
	}

	void name_changed ()
	{
		setup_sensitivity();
	}

	void add (SPLEntry e)
	{
		wxListItem item;
		item.SetId (_list->GetItemCount());
		long const N = _list->InsertItem (item);
		set_item (N, e);
	}

	void set_item (long N, SPLEntry e)
	{
		_list->SetItem (N, 0, std_to_wx(e.name));
		_list->SetItem (N, 1, std_to_wx(e.id));
		_list->SetItem (N, 2, std_to_wx(e.kind->name()));
		_list->SetItem (N, 3, e.encrypted ? S_("Question|Y") : S_("Question|N"));
	}

	void setup_sensitivity ()
	{
		bool const have_list = static_cast<bool>(_playlist);
		int const num_selected = _list->GetSelectedItemCount ();
		long int selected = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		_name->Enable (have_list);
		_save_name->Enable(_playlist && _playlist->name() != wx_to_std(_name->GetValue()));
		_list->Enable (have_list);
		_up->Enable (have_list && selected > 0);
		_down->Enable (have_list && selected != -1 && selected < (_list->GetItemCount() - 1));
		_add->Enable (have_list);
		_remove->Enable (have_list && num_selected > 0);
	}

	void add_clicked ()
	{
		int const r = _content_dialog->ShowModal ();
		if (r == wxID_OK) {
			auto content = _content_dialog->selected ();
			if (content) {
				SPLEntry e (content);
				add (e);
				DCPOMATIC_ASSERT (_playlist);
				_playlist->add (e);
			}
		}
	}

	void up_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s < 1) {
			return;
		}

		DCPOMATIC_ASSERT (_playlist);

		_playlist->swap(s, s - 1);

		set_item (s - 1, (*_playlist)[s-1]);
		set_item (s, (*_playlist)[s]);
	}

	void down_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s > (_list->GetItemCount() - 1)) {
			return;
		}

		DCPOMATIC_ASSERT (_playlist);

		_playlist->swap(s, s + 1);

		set_item (s + 1, (*_playlist)[s+1]);
		set_item (s, (*_playlist)[s]);
	}

	void remove_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			return;
		}

		DCPOMATIC_ASSERT (_playlist);
		_playlist->remove (s);
		_list->DeleteItem (s);
	}

	ContentDialog* _content_dialog;
	wxBoxSizer* _sizer;
	wxTextCtrl* _name;
	Button* _save_name;
	wxListCtrl* _list;
	wxButton* _up;
	wxButton* _down;
	wxButton* _add;
	wxButton* _remove;
	shared_ptr<SignalSPL> _playlist;
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (nullptr, wxID_ANY, title)
		, _content_dialog (new ContentDialog(this))
		, _config_dialog (nullptr)
	{
		auto bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this, wxID_ANY);
		auto sizer = new wxBoxSizer (wxVERTICAL);

		_playlist_list = new PlaylistList (overall_panel, _content_dialog);
		_playlist_content = new PlaylistContent (overall_panel, _content_dialog);

		sizer->Add (_playlist_list->sizer());
		sizer->Add (_playlist_content->sizer());

		overall_panel->SetSizer (sizer);

		_playlist_list->Edit.connect (bind(&DOMFrame::change_playlist, this, _1));

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this), wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this), wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);

		_config_changed_connection = Config::instance()->Changed.connect(boost::bind(&DOMFrame::config_changed, this));
	}

private:

	void file_exit ()
	{
		/* false here allows the close handler to veto the close request */
		Close (false);
	}

	void help_about ()
	{
		auto d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_playlist_editor_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void change_playlist (shared_ptr<SignalSPL> playlist)
	{
		auto old = _playlist_content->playlist ();
		if (old) {
			save_playlist (old);
		}
		_playlist_content->set (playlist);
	}

	void setup_menu (wxMenuBar* m)
	{
		auto file = new wxMenu;
#ifdef __WXOSX__
		file->Append (wxID_EXIT, _("&Exit"));
#else
		file->Append (wxID_EXIT, _("&Quit"));
#endif

#ifndef __WXOSX__
		auto edit = new wxMenu;
		edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		auto help = new wxMenu;
#ifdef __WXOSX__
		help->Append (wxID_ABOUT, _("About DCP-o-matic"));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif

		m->Append (file, _("&File"));
#ifndef __WXOSX__
		m->Append (edit, _("&Edit"));
#endif
		m->Append (help, _("&Help"));
	}


	void config_changed ()
	{
		try {
			Config::instance()->write_config();
		} catch (exception& e) {
			error_dialog (
				this,
				wxString::Format (
					_("Could not write to config file at %s.  Your changes have not been saved."),
					std_to_wx (Config::instance()->cinemas_file().string()).data()
					)
				);
		}
	}

	ContentDialog* _content_dialog;
	PlaylistList* _playlist_list;
	PlaylistContent* _playlist_content;
	wxPreferencesEditor* _config_dialog;
	boost::signals2::scoped_connection _config_changed_connection;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (nullptr)
	{}

private:

	bool OnInit () override
	try
	{
		wxInitAllImageHandlers ();
		SetAppName (_("DCP-o-matic Playlist Editor"));

		if (!wxApp::OnInit()) {
			return false;
		}

#ifdef DCPOMATIC_LINUX
		unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
		make_foreground_application ();
#endif

		dcpomatic_setup_path_encoding ();

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including filters etc.
		   which will now be internationalised correctly.
		*/
		dcpomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		_frame = new DOMFrame (_("DCP-o-matic Playlist Editor"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		_frame->Show ();

		signal_manager = new wxSignalManager (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, _("DCP-o-matic could not start"), std_to_wx(e.what()));
		return true;
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop () override
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str ())
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s.\n\n") + " " + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}

		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
	{
		error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	DOMFrame* _frame;
};

IMPLEMENT_APP (App)
