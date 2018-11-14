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
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>

using std::exception;

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
		_list->AppendColumn (_("Type"), wxLIST_FORMAT_LEFT, 75);
		_list->AppendColumn (_("Format"), wxLIST_FORMAT_LEFT, 75);
		_list->AppendColumn (_("Encrypted"), wxLIST_FORMAT_LEFT, 90);
		_list->AppendColumn (_("Skippable"), wxLIST_FORMAT_LEFT, 90);
		_list->AppendColumn (_("Disable timeline"), wxLIST_FORMAT_LEFT, 125);
		_list->AppendColumn (_("Stop after play"), wxLIST_FORMAT_LEFT, 125);

		/*
		wxImageList* images = new wxImageList (16, 16);
		wxIcon icon;
		icon.LoadFile ("test.png", wxBITMAP_TYPE_PNG);
		images->Add (icon);
		_list->SetImageList (images, wxIMAGE_LIST_SMALL);
		*/

		wxListItem item;
		item.SetId (0);
		item.SetImage (0);
		_list->InsertItem (item);

		main_sizer->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

		wxBoxSizer* button_sizer = new wxBoxSizer (wxVERTICAL);
		_up = new wxButton (overall_panel, wxID_ANY, _("Up"));
		button_sizer->Add (_up, 0, wxALL, DCPOMATIC_SIZER_GAP);

		main_sizer->Add (button_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
		overall_panel->SetSizer (main_sizer);

		setup_sensitivity ();
	}

	void setup_sensitivity ()
	{

	}

	wxListCtrl* _list;
	wxButton* _up;
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
