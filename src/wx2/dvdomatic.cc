#include <wx/wx.h>
#include "lib/util.h"
#include "lib/film.h"
#include "film_viewer.h"
#include "film_editor.h"

enum {
	ID_Quit = 1,
};

class Frame : public wxFrame
{
public:
	Frame (wxString const & title, wxPoint const & pos, wxSize const & size)
		: wxFrame (NULL, -1, title, pos, size)
	{
		wxMenuBar* bar = new wxMenuBar;
		
		wxMenu *menu_file = new wxMenu;
		menu_file->Append (ID_Quit, _("&Quit"));

		bar->Append (menu_file, _("&File"));

		SetMenuBar (bar);

		CreateStatusBar ();
		SetStatusText (_("Welcome to DVD-o-matic!"));
	}
	
	void OnQuit (wxCommandEvent& event)
	{
		Close (true);
	}
};

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit ()) {
			return false;
		}

		wxInitAllImageHandlers ();
		
		dvdomatic_setup ();

		Film* film = new Film ("/home/carl/DCP/BitHarvest");
		
		Frame* frame = new Frame (_("DVD-o-matic"), wxPoint (50, 50), wxSize(450, 350));
		frame->Show (true);
		
		frame->Connect (
			ID_Quit, wxEVT_COMMAND_MENU_SELECTED,
			(wxObjectEventFunction) &Frame::OnQuit
			);

		FilmEditor* editor = new FilmEditor (film, frame);
		editor->Show (true);
		FilmViewer* viewer = new FilmViewer (film, frame);
		viewer->load_thumbnail (22);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (editor, 0);
		main_sizer->Add (viewer->get_widget (), 1, wxEXPAND);
		frame->SetSizer (main_sizer);

//		frame->Add (viewer->get_widget ());

		SetTopWindow (frame);
		return true;
	}
};

IMPLEMENT_APP (App)

