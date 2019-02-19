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

#include "../wx/wx_util.h"
#include "../wx/wx_signal_manager.h"
#include "../wx/content_view.h"
#include "../wx/dcpomatic_button.h"
#include "../lib/util.h"
#include "../lib/config.h"
#include "../lib/cross.h"
#include "../lib/film.h"
#include "../lib/dcp_content.h"
#include "../lib/spl_entry.h"
#include "../lib/spl.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif

using std::exception;
using std::cout;
using std::string;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::bind;
using boost::dynamic_pointer_cast;

class ContentDialog : public wxDialog, public ContentStore
{
public:
	ContentDialog (wxWindow* parent)
		: wxDialog (parent, wxID_ANY, _("Add content"), wxDefaultPosition, wxSize(800, 640))
		, _content_view (new ContentView(this))
	{
		_content_view->update ();

		wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
		SetSizer (overall_sizer);

		overall_sizer->Add (_content_view, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
		if (buttons) {
			overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
		}

		overall_sizer->Layout ();
	}

	shared_ptr<Content> selected () const
	{
		return _content_view->selected ();
	}

	shared_ptr<Content> get (string digest) const
	{
		return _content_view->get (digest);
	}

private:
	ContentView* _content_view;
};

class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (0, -1, title)
		, _content_dialog (new ContentDialog(this))
	{
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);
		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);

		_list = new wxListCtrl (
			overall_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL
			);

		_list->AppendColumn (_("Name"), wxLIST_FORMAT_LEFT, 400);
		_list->AppendColumn (_("CPL"), wxLIST_FORMAT_LEFT, 350);
		_list->AppendColumn (_("Type"), wxLIST_FORMAT_CENTRE, 100);
		_list->AppendColumn (_("Format"), wxLIST_FORMAT_CENTRE, 75);
		_list->AppendColumn (_("Encrypted"), wxLIST_FORMAT_CENTRE, 90);
		_list->AppendColumn (_("Skippable"), wxLIST_FORMAT_CENTRE, 90);
		_list->AppendColumn (_("Disable timeline"), wxLIST_FORMAT_CENTRE, 125);
		_list->AppendColumn (_("Stop after play"), wxLIST_FORMAT_CENTRE, 125);

		wxImageList* images = new wxImageList (16, 16);
		wxIcon tick_icon;
		wxIcon no_tick_icon;
#ifdef DCPOMATIX_OSX
		tick_icon.LoadFile ("tick.png", wxBITMAP_TYPE_PNG_RESOURCE);
		no_tick_icon.LoadFile ("no_tick.png", wxBITMAP_TYPE_PNG_RESOURCE);
#else
		boost::filesystem::path tick_path = shared_path() / "tick.png";
		tick_icon.LoadFile (std_to_wx(tick_path.string()));
		boost::filesystem::path no_tick_path = shared_path() / "no_tick.png";
		no_tick_icon.LoadFile (std_to_wx(no_tick_path.string()));
#endif
		images->Add (tick_icon);
		images->Add (no_tick_icon);

		_list->SetImageList (images, wxIMAGE_LIST_SMALL);

		main_sizer->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

		wxBoxSizer* button_sizer = new wxBoxSizer (wxVERTICAL);
		_up = new Button (overall_panel, _("Up"));
		_down = new Button (overall_panel, _("Down"));
		_add = new Button (overall_panel, _("Add"));
		_remove = new Button (overall_panel, _("Remove"));
		_save = new Button (overall_panel, _("Save playlist"));
		_load = new Button (overall_panel, _("Load playlist"));
		button_sizer->Add (_up, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_down, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_add, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_remove, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_save, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_load, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

		main_sizer->Add (button_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
		overall_panel->SetSizer (main_sizer);

		_list->Bind (wxEVT_LEFT_DOWN, bind(&DOMFrame::list_left_click, this, _1));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&DOMFrame::selection_changed, this));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&DOMFrame::selection_changed, this));
		_up->Bind (wxEVT_BUTTON, bind(&DOMFrame::up_clicked, this));
		_down->Bind (wxEVT_BUTTON, bind(&DOMFrame::down_clicked, this));
		_add->Bind (wxEVT_BUTTON, bind(&DOMFrame::add_clicked, this));
		_remove->Bind (wxEVT_BUTTON, bind(&DOMFrame::remove_clicked, this));
		_save->Bind (wxEVT_BUTTON, bind(&DOMFrame::save_clicked, this));
		_load->Bind (wxEVT_BUTTON, bind(&DOMFrame::load_clicked, this));

		setup_sensitivity ();
	}

private:

	void add (SPLEntry e)
	{
		wxListItem item;
		item.SetId (_list->GetItemCount());
		long const N = _list->InsertItem (item);
		set_item (N, e);
	}

	void selection_changed ()
	{
		setup_sensitivity ();
	}

	void set_item (long N, SPLEntry e)
	{
		_list->SetItem (N, 0, std_to_wx(e.name));
		_list->SetItem (N, 1, std_to_wx(e.id));
		_list->SetItem (N, 2, std_to_wx(dcp::content_kind_to_string(e.kind)));
		_list->SetItem (N, 3, e.type == SPLEntry::DCP ? _("DCP") : _("E-cinema"));
		_list->SetItem (N, 4, e.encrypted ? _("Y") : _("N"));
		_list->SetItem (N, COLUMN_SKIPPABLE, wxEmptyString, e.skippable ? 0 : 1);
		_list->SetItem (N, COLUMN_DISABLE_TIMELINE, wxEmptyString, e.disable_timeline ? 0 : 1);
		_list->SetItem (N, COLUMN_STOP_AFTER_PLAY, wxEmptyString, e.stop_after_play ? 0 : 1);
	}

	void setup_sensitivity ()
	{
		int const num_selected = _list->GetSelectedItemCount ();
		long int selected = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		_up->Enable (selected > 0);
		_down->Enable (selected != -1 && selected < (_list->GetItemCount() - 1));
		_remove->Enable (num_selected > 0);
	}

	void list_left_click (wxMouseEvent& ev)
	{
		int flags;
		long item = _list->HitTest (ev.GetPosition(), flags, 0);
		int x = ev.GetPosition().x;
		optional<int> column;
		for (int i = 0; i < _list->GetColumnCount(); ++i) {
			x -= _list->GetColumnWidth (i);
			if (x < 0) {
				column = i;
				break;
			}
		}

		if (item != -1 && column) {
			switch (*column) {
			case COLUMN_SKIPPABLE:
				_playlist[item].skippable = !_playlist[item].skippable;
				break;
			case COLUMN_DISABLE_TIMELINE:
				_playlist[item].disable_timeline = !_playlist[item].disable_timeline;
				break;
			case COLUMN_STOP_AFTER_PLAY:
				_playlist[item].stop_after_play = !_playlist[item].stop_after_play;
				break;
			default:
				ev.Skip ();
			}
			set_item (item, _playlist[item]);
		} else {
			ev.Skip ();
		}
	}

	void add_clicked ()
	{
		int const r = _content_dialog->ShowModal ();
		if (r == wxID_OK) {
			shared_ptr<Content> content = _content_dialog->selected ();
			if (content) {
				SPLEntry e (content);
				add (e);
				_playlist.add (e);
			}
		}
	}

	void up_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s < 1) {
			return;
		}

		SPLEntry tmp = _playlist[s];
		_playlist[s] = _playlist[s-1];
		_playlist[s-1] = tmp;

		set_item (s - 1, _playlist[s-1]);
		set_item (s, _playlist[s]);
	}

	void down_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s > (_list->GetItemCount() - 1)) {
			return;
		}

		SPLEntry tmp = _playlist[s];
		_playlist[s] = _playlist[s+1];
		_playlist[s+1] = tmp;

		set_item (s + 1, _playlist[s+1]);
		set_item (s, _playlist[s]);
	}

	void remove_clicked ()
	{
		long int s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			return;
		}

		_playlist.remove (s);
		_list->DeleteItem (s);
	}

	void save_clicked ()
	{
		Config* c = Config::instance ();
		wxString default_dir = c->player_playlist_directory() ? std_to_wx(c->player_playlist_directory()->string()) : wxString(wxEmptyString);
		wxFileDialog* d = new wxFileDialog (this, _("Select playlist file"), default_dir, wxEmptyString, wxT("XML files (*.xml)|*.xml"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (d->ShowModal() == wxID_OK) {
			boost::filesystem::path file = wx_to_std (d->GetPath());
			file.replace_extension (".xml");
			_playlist.write (file);
		}
	}

	void load_clicked ()
	{
		Config* c = Config::instance ();
		wxString default_dir = c->player_playlist_directory() ? std_to_wx(c->player_playlist_directory()->string()) : wxString(wxEmptyString);
		wxFileDialog* d = new wxFileDialog (this, _("Select playlist file"), default_dir, wxEmptyString, wxT("XML files (*.xml)|*.xml"));
		if (d->ShowModal() == wxID_OK) {
			_list->DeleteAllItems ();
			_playlist.read (wx_to_std(d->GetPath()), _content_dialog);
			if (!_playlist.missing()) {
				_list->DeleteAllItems ();
				BOOST_FOREACH (SPLEntry i, _playlist.get()) {
					add (i);
				}
			} else {
				error_dialog (this, _("Some content in this playlist was not found."));
			}
		}
	}

	wxListCtrl* _list;
	wxButton* _up;
	wxButton* _down;
	wxButton* _add;
	wxButton* _remove;
	wxButton* _save;
	wxButton* _load;
	SPL _playlist;
	ContentDialog* _content_dialog;

	enum {
		COLUMN_SKIPPABLE = 5,
		COLUMN_DISABLE_TIMELINE = 6,
		COLUMN_STOP_AFTER_PLAY = 7
	};
};

/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (0)
	{}

private:

	bool OnInit ()
	try
	{
		SetAppName (_("DCP-o-matic KDM Creator"));

		if (!wxApp::OnInit()) {
			return false;
		}

#ifdef DCPOMATIC_LINUX
		unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef __WXOSX__
		ProcessSerialNumber serial;
		GetCurrentProcess (&serial);
		TransformProcessType (&serial, kProcessTransformToForegroundApplication);
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
	bool OnExceptionInMainLoop ()
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

	void OnUnhandledException ()
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
