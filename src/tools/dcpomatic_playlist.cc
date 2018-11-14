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
#include "../lib/util.h"
#include "../lib/config.h"
#include "../lib/cross.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>

using std::exception;
using std::cout;
using boost::optional;

class PlaylistEntry
{
public:
	std::string name;
	std::string cpl_id;
	dcp::ContentKind kind;
	enum Type {
		DCP,
		ECINEMA
	};
	Type type;
	bool encrypted;
	bool skippable;
	bool disable_timeline;
	bool stop_after_play;
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (0, -1, title)
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
		_list->AppendColumn (_("CPL"), wxLIST_FORMAT_LEFT, 400);
		_list->AppendColumn (_("Type"), wxLIST_FORMAT_CENTRE, 75);
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
		_up = new wxButton (overall_panel, wxID_ANY, _("Up"));
		_down = new wxButton (overall_panel, wxID_ANY, _("Down"));
		_add = new wxButton (overall_panel, wxID_ANY, _("Add"));
		_remove = new wxButton (overall_panel, wxID_ANY, _("Remove"));
		_save = new wxButton (overall_panel, wxID_ANY, _("Save playlist"));
		_load = new wxButton (overall_panel, wxID_ANY, _("Load playlist"));
		button_sizer->Add (_up, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_down, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_add, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_remove, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_save, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		button_sizer->Add (_load, 0, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

		main_sizer->Add (button_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
		overall_panel->SetSizer (main_sizer);

		_list->Bind (wxEVT_LEFT_DOWN, bind(&DOMFrame::list_left_click, this, _1));

		PlaylistEntry pe;
		pe.name = "Shit";
		pe.cpl_id = "sh-1t";
		pe.kind = dcp::FEATURE;
		pe.type = PlaylistEntry::ECINEMA;
		pe.encrypted = true;
		pe.disable_timeline = false;
		pe.stop_after_play = true;
		add (pe);

		setup_sensitivity ();
	}

private:

	void add (PlaylistEntry e)
	{
		wxListItem item;
		item.SetId (0);
		long const N = _list->InsertItem (item);
		set_item (N, e);
		_playlist.push_back (e);
	}

	void set_item (long N, PlaylistEntry e)
	{
		_list->SetItem (N, 0, std_to_wx(e.name));
		_list->SetItem (N, 1, std_to_wx(e.cpl_id));
		_list->SetItem (N, 2, std_to_wx(dcp::content_kind_to_string(e.kind)));
		_list->SetItem (N, 3, e.type == PlaylistEntry::DCP ? _("DCP") : _("E-cinema"));
		_list->SetItem (N, 4, e.encrypted ? _("Y") : _("N"));
		_list->SetItem (N, COLUMN_SKIPPABLE, wxEmptyString, e.skippable ? 0 : 1);
		_list->SetItem (N, COLUMN_DISABLE_TIMELINE, wxEmptyString, e.disable_timeline ? 0 : 1);
		_list->SetItem (N, COLUMN_STOP_AFTER_PLAY, wxEmptyString, e.stop_after_play ? 0 : 1);
	}

	void setup_sensitivity ()
	{
		int const selected = _list->GetSelectedItemCount ();
		_up->Enable (selected > 0);
		_down->Enable (selected > 0);
		_remove->Enable (selected > 0);
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

	wxListCtrl* _list;
	wxButton* _up;
	wxButton* _down;
	wxButton* _add;
	wxButton* _remove;
	wxButton* _save;
	wxButton* _load;
	std::vector<PlaylistEntry> _playlist;

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

		_frame = new DOMFrame (_("DCP-o-matic KDM Creator"));
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
