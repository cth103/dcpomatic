/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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
#include "wx/editable_list.h"
#include "wx/id.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/null_log.h"
#include "lib/util.h"
#include "lib/variant.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/cmdline.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#include <iostream>


using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


enum {
	ID_file_open = DCPOMATIC_MAIN_MENU,
	ID_file_save,
};


class AssetPanel : public wxPanel
{
public:
	AssetPanel(wxWindow* parent, shared_ptr<dcp::ReelAsset> asset)
		: wxPanel(parent, wxID_ANY)
		, _asset(asset)
	{
		auto sizer = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

		int r = 0;

		add_label_to_sizer(sizer, this, _("Annotation text"), true, wxGBPosition(r, 0));
		_annotation_text = new wxTextCtrl(this, wxID_ANY, std_to_wx(asset->annotation_text().get_value_or("")), wxDefaultPosition, wxSize(600, -1));
		sizer->Add(_annotation_text, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer(sizer, this, _("Entry point"), true, wxGBPosition(r, 0));
		_entry_point = new wxSpinCtrl(this, wxID_ANY);
		sizer->Add(_entry_point, wxGBPosition(r, 1), wxDefaultSpan);
		++r;

		add_label_to_sizer(sizer, this, _("Duration"), true, wxGBPosition(r, 0));
		_duration = new wxSpinCtrl(this, wxID_ANY);
		sizer->Add(_duration, wxGBPosition(r, 1), wxDefaultSpan);
		++r;

		add_label_to_sizer(sizer, this, _("Intrinsic duration"), true, wxGBPosition(r, 0));
		auto intrinsic_duration = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
		sizer->Add(intrinsic_duration, wxGBPosition(r, 1), wxDefaultSpan);
		++r;

		auto space = new wxBoxSizer(wxVERTICAL);
		space->Add(sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
		SetSizerAndFit(space);

		_entry_point->SetRange(0, 259200);
		_entry_point->SetValue(asset->entry_point().get_value_or(0));

		_duration->SetRange(0, 259200);
		_duration->SetValue(asset->duration().get_value_or(0));

		intrinsic_duration->SetValue(wxString::Format(char_to_wx("%ld"), asset->intrinsic_duration()));

		_annotation_text->Bind(wxEVT_TEXT, boost::bind(&AssetPanel::annotation_text_changed, this));
		_entry_point->Bind(wxEVT_SPINCTRL, boost::bind(&AssetPanel::entry_point_changed, this));
		_duration->Bind(wxEVT_SPINCTRL, boost::bind(&AssetPanel::duration_changed, this));
	}

private:
	void annotation_text_changed()
	{
		_asset->set_annotation_text(wx_to_std(_annotation_text->GetValue()));
	}

	void entry_point_changed()
	{
		_asset->set_entry_point(_entry_point->GetValue());
		auto const fixed_duration = std::min(_asset->intrinsic_duration() - _asset->entry_point().get_value_or(0LL), _asset->duration().get_value_or(_asset->intrinsic_duration()));
		_duration->SetValue(fixed_duration);
		_asset->set_duration(fixed_duration);
	}

	void duration_changed()
	{
		_asset->set_duration(_duration->GetValue());
		auto const fixed_entry_point = std::min(_asset->intrinsic_duration() - _asset->duration().get_value_or(_asset->intrinsic_duration()), _asset->entry_point().get_value_or(0LL));
		_entry_point->SetValue(fixed_entry_point);
		_asset->set_entry_point(fixed_entry_point);
	}

	wxTextCtrl* _annotation_text = nullptr;
	wxSpinCtrl* _entry_point = nullptr;
	wxSpinCtrl* _duration = nullptr;
	shared_ptr<dcp::ReelAsset> _asset;
};


class ReelEditor : public wxDialog
{
public:
	ReelEditor(wxWindow* parent)
		: wxDialog(parent, wxID_ANY, _("Edit reel"))
	{
		_sizer = new wxBoxSizer(wxVERTICAL);
		_notebook = new wxNotebook(this, wxID_ANY);
		_sizer->Add(_notebook, wxEXPAND | wxALL, 1, DCPOMATIC_DIALOG_BORDER);
		SetSizerAndFit(_sizer);
	}

	optional<shared_ptr<dcp::Reel>> get() {
		return _reel;
	}

	void set(shared_ptr<dcp::Reel> reel)
	{
		_reel = reel;

		_notebook->DeleteAllPages();
		if (_reel->main_picture()) {
			_notebook->AddPage(new AssetPanel(_notebook, _reel->main_picture()), _("Picture"));
		}
		if (_reel->main_sound()) {
			_notebook->AddPage(new AssetPanel(_notebook, _reel->main_sound()), _("Sound"));
		}
		if (_reel->main_subtitle()) {
			_notebook->AddPage(new AssetPanel(_notebook, _reel->main_subtitle()), _("Subtitle"));
		}

		_sizer->Layout();
		_sizer->SetSizeHints(this);
	}

private:
	wxNotebook* _notebook = nullptr;
	wxSizer* _sizer = nullptr;
	shared_ptr<dcp::Reel> _reel;
};


class CPLPanel : public wxPanel
{
public:
	CPLPanel(wxWindow* parent, shared_ptr<dcp::CPL> cpl)
		: wxPanel(parent, wxID_ANY)
		, _cpl(cpl)
	{
		auto sizer = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

		int r = 0;

		add_label_to_sizer(sizer, this, _("Annotation text"), true, wxGBPosition(r, 0));
		_annotation_text = new wxTextCtrl(this, wxID_ANY, std_to_wx(cpl->annotation_text().get_value_or("")), wxDefaultPosition, wxSize(600, -1));
		sizer->Add(_annotation_text, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer(sizer, this, _("Issuer"), true, wxGBPosition(r, 0));
		_issuer = new wxTextCtrl(this, wxID_ANY, std_to_wx(cpl->issuer()), wxDefaultPosition, wxSize(600, -1));
		sizer->Add(_issuer, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer(sizer, this, _("Creator"), true, wxGBPosition(r, 0));
		_creator = new wxTextCtrl(this, wxID_ANY, std_to_wx(cpl->creator()), wxDefaultPosition, wxSize(600, -1));
		sizer->Add(_creator, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer(sizer, this, _("Content title text"), true, wxGBPosition(r, 0));
		_content_title_text = new wxTextCtrl(this, wxID_ANY, std_to_wx(cpl->content_title_text()), wxDefaultPosition, wxSize(600, -1));
		sizer->Add(_content_title_text, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer(sizer, this, _("Reels"), true, wxGBPosition(r, 0));
		_reels = new EditableList<shared_ptr<dcp::Reel>, ReelEditor>(
			this,
			{ EditableListColumn("Name", 600, true) },
			[this]() { return _cpl->reels(); },
			[this](vector<shared_ptr<dcp::Reel>> reels) {
				_cpl->set(reels);
			},
			[](shared_ptr<dcp::Reel> reel, int) {
				return reel->id();
			},
			EditableListTitle::INVISIBLE,
			EditableListButton::EDIT
		);
		sizer->Add(_reels, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);

		auto space = new wxBoxSizer(wxVERTICAL);
		space->Add(sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
		SetSizerAndFit(space);

		_annotation_text->Bind(wxEVT_TEXT, boost::bind(&CPLPanel::annotation_text_changed, this));
		_issuer->Bind(wxEVT_TEXT, boost::bind(&CPLPanel::issuer_changed, this));
		_creator->Bind(wxEVT_TEXT, boost::bind(&CPLPanel::creator_changed, this));
		_content_title_text->Bind(wxEVT_TEXT, boost::bind(&CPLPanel::content_title_text_changed, this));
	}

private:
	void annotation_text_changed()
	{
		_cpl->set_annotation_text(wx_to_std(_annotation_text->GetValue()));
	}

	void issuer_changed()
	{
		_cpl->set_issuer(wx_to_std(_issuer->GetValue()));
	}

	void creator_changed()
	{
		_cpl->set_creator(wx_to_std(_creator->GetValue()));
	}

	void content_title_text_changed()
	{
		_cpl->set_content_title_text(wx_to_std(_content_title_text->GetValue()));
	}

	std::shared_ptr<dcp::CPL> _cpl;
	wxTextCtrl* _annotation_text = nullptr;
	wxTextCtrl* _issuer = nullptr;
	wxTextCtrl* _creator = nullptr;
	wxTextCtrl* _content_title_text = nullptr;
	EditableList<shared_ptr<dcp::Reel>, ReelEditor>* _reels;
};


class DummyPanel : public wxPanel
{
public:
	DummyPanel(wxWindow* parent)
		: wxPanel(parent, wxID_ANY)
	{
		auto sizer = new wxBoxSizer(wxVERTICAL);
		add_label_to_sizer(sizer, this, _("Open a DCP using File -> Open"), false);
		auto space = new wxBoxSizer(wxVERTICAL);
		space->Add(sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
		SetSizerAndFit(space);
	}
};


class DOMFrame : public wxFrame
{
public:
	DOMFrame ()
		: wxFrame(nullptr, -1, variant::wx::dcpomatic_editor())
		, _main_sizer(new wxBoxSizer(wxVERTICAL))
	{
		dcpomatic_log = make_shared<NullLog>();

#if defined(DCPOMATIC_WINDOWS)
		maybe_open_console();
		std::cout << variant::dcpomatic_editor() << " is starting." << "\n";
#endif

		auto bar = new wxMenuBar;
		setup_menu(bar);
		SetMenuBar(bar);

#ifdef DCPOMATIC_WINDOWS
		SetIcon(wxIcon(std_to_wx("id")));
#endif

		Bind(wxEVT_MENU, boost::bind(&DOMFrame::file_open, this), ID_file_open);
		Bind(wxEVT_MENU, boost::bind(&DOMFrame::file_save, this), ID_file_save);
		Bind(wxEVT_MENU, boost::bind(&DOMFrame::file_exit, this), wxID_EXIT);
		Bind(wxEVT_MENU, boost::bind(&DOMFrame::help_about, this), wxID_ABOUT);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		_overall_panel = new wxPanel (this, wxID_ANY);

		auto sizer = new wxBoxSizer(wxVERTICAL);

		_notebook = new wxNotebook(_overall_panel, wxID_ANY);
		_notebook->AddPage(new DummyPanel(_notebook), _("CPL"));

		sizer->Add(_notebook, 1, wxEXPAND);
		_overall_panel->SetSizerAndFit(sizer);
	}

	void load_dcp (boost::filesystem::path path)
	{
		try {
			_dcp = dcp::DCP(path);
			_dcp->read();
		} catch (std::runtime_error& e) {
			error_dialog(this, _("Could not load DCP"), std_to_wx(e.what()));
			return;
		}

		_notebook->DeleteAllPages();
		for (auto cpl: _dcp->cpls()) {
			_notebook->AddPage(new CPLPanel(_notebook, cpl), wx_to_std(cpl->annotation_text().get_value_or(cpl->id())));
		}
	}

private:

	void setup_menu (wxMenuBar* m)
	{
		_file_menu = new wxMenu;
		_file_menu->Append (ID_file_open, _("&Open...\tCtrl-O"));
		_file_menu->AppendSeparator ();
		_file_menu->Append (ID_file_save, _("&Save\tCtrl-S"));
		_file_menu->AppendSeparator ();
#ifdef __WXOSX__
		_file_menu->Append (wxID_EXIT, _("&Exit"));
#else
		_file_menu->Append (wxID_EXIT, _("&Quit"));
#endif

		auto help = new wxMenu;
#ifdef __WXOSX__
		help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic_editor(_("About %s")));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif

		m->Append (_file_menu, _("&File"));
		m->Append (help, _("&Help"));
	}

	void file_open ()
	{
		auto d = wxStandardPaths::Get().GetDocumentsDir();
		wxDirDialog dialog(this, _("Select DCP to open"), d, wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);

		int r;
		while (true) {
			r = dialog.ShowModal();
			if (r == wxID_OK && dialog.GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK) {
			boost::filesystem::path const dcp(wx_to_std(dialog.GetPath()));
			load_dcp (dcp);
		}
	}

	void file_save ()
	{
		_dcp->write_xml();
	}

	void file_exit ()
	{
		Close ();
	}

	void help_about ()
	{
		AboutDialog dialog(this);
		dialog.ShowModal();
	}

	wxPanel* _overall_panel = nullptr;
	wxMenu* _file_menu = nullptr;
	wxSizer* _main_sizer = nullptr;
	wxNotebook* _notebook = nullptr;
	optional<dcp::DCP> _dcp;
};


static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to edit", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit () override
	{
		wxSplashScreen* splash;
		try {
			wxInitAllImageHandlers ();

			splash = maybe_show_splash ();

			SetAppName(variant::wx::dcpomatic_editor());

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

			signal_manager = new wxSignalManager (this);

			_frame = new DOMFrame ();
			SetTopWindow (_frame);
			_frame->Maximize ();
			if (splash) {
				splash->Destroy ();
				splash = nullptr;
			}
			_frame->Show ();

			if (_dcp_to_load) {
				_frame->load_dcp(*_dcp_to_load);
			}

			Bind (wxEVT_IDLE, boost::bind (&App::idle, this));
		}
		catch (exception& e)
		{
			if (splash) {
				splash->Destroy ();
			}
			error_dialog(nullptr, variant::wx::insert_dcpomatic_editor(_("%s could not start.")), std_to_wx(e.what()));
		}

		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
	{
		if (parser.GetParamCount() > 0) {
			_dcp_to_load = wx_to_std(parser.GetParam(0));
		}

		return true;
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s\n\n%s"),
					std_to_wx(e.what()),
					wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), wx::report_problem()));
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop () override
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
	{
		report_exception ();
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	DOMFrame* _frame = nullptr;
	optional<boost::filesystem::path> _dcp_to_load;
};


IMPLEMENT_APP (App)
