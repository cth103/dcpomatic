/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/dir_picker_ctrl.h"
#include "wx/editable_list.h"
#include "wx/wx_signal_manager.h"
#include "lib/combine_dcp_job.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include <dcp/combine.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/filepicker.h>
DCPOMATIC_ENABLE_WARNINGS
#include <wx/wx.h>
#include <boost/bind/bind.hpp>
#include <boost/filesystem.hpp>
#include <exception>


using std::dynamic_pointer_cast;
using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static string
display_string (boost::filesystem::path p, int)
{
	return p.filename().string();
}


class DirDialogWrapper : public wxDirDialog
{
public:
	DirDialogWrapper (wxWindow* parent)
		: wxDirDialog (parent, _("Choose a DCP folder"), wxT(""), wxDD_DIR_MUST_EXIST)
	{

	}

	boost::filesystem::path get () const
	{
		return boost::filesystem::path(wx_to_std(GetPath()));
	}

	void set (boost::filesystem::path)
	{
		/* Not used */
	}
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (nullptr, -1, title)
	{
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this);
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (overall_panel, 1, wxEXPAND);
		SetSizer (s);

		vector<EditableListColumn> columns;
		columns.push_back(EditableListColumn(_("Input DCP"), 600, true));

		_input = new EditableList<boost::filesystem::path, DirDialogWrapper>(
			overall_panel,
			columns,
			boost::bind(&DOMFrame::inputs, this),
			boost::bind(&DOMFrame::set_inputs, this, _1),
			&display_string,
			false,
			true
			);

		auto output = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		output->AddGrowableCol (1, 1);

		add_label_to_sizer (output, overall_panel, _("Annotation text"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_annotation_text = new wxTextCtrl (overall_panel, wxID_ANY, wxT(""));
		output->Add (_annotation_text, 1, wxEXPAND);

		add_label_to_sizer (output, overall_panel, _("Output DCP folder"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_output = new DirPickerCtrl (overall_panel);
		output->Add (_output, 1, wxEXPAND);

		_combine = new Button (overall_panel, _("Combine"));

		auto sizer = new wxBoxSizer (wxVERTICAL);
		sizer->Add (_input, 1, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
		sizer->Add (output, 0, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
		sizer->Add (_combine, 0, wxALL | wxALIGN_RIGHT, DCPOMATIC_DIALOG_BORDER);
		overall_panel->SetSizer (sizer);
		Fit ();
		SetSize (768, GetSize().GetHeight() + 32);

		_combine->Bind (wxEVT_BUTTON, boost::bind(&DOMFrame::combine, this));
		_output->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind(&DOMFrame::setup_sensitivity, this));

		setup_sensitivity ();
	}

private:
	void set_inputs (vector<boost::filesystem::path> inputs)
	{
		_inputs = inputs;
	}

	vector<boost::filesystem::path> inputs () const
	{
		return _inputs;
	}

	void combine ()
	{
		using namespace boost::filesystem;

		path const output = wx_to_std(_output->GetPath());

		if (is_directory(output) && !is_empty(output)) {
			if (!confirm_dialog (
				    this,
				    std_to_wx (
					    String::compose(wx_to_std(_("The directory %1 already exists and is not empty.  "
									"Are you sure you want to use it?")),
							    output.string())
					    )
				    )) {
				return;
			}
		} else if (is_regular_file(output)) {
			error_dialog (
				this,
				String::compose (wx_to_std(_("%1 already exists as a file, so you cannot use it for a DCP.")), output.string())
				);
			return;
		}

		auto jm = JobManager::instance ();
		jm->add (make_shared<CombineDCPJob>(_inputs, output, wx_to_std(_annotation_text->GetValue())));
		bool const ok = display_progress (_("DCP-o-matic Combine"), _("Combining DCPs"));
		if (!ok) {
			return;
		}

		DCPOMATIC_ASSERT (!jm->get().empty());
		auto last = dynamic_pointer_cast<CombineDCPJob> (jm->get().back());
		DCPOMATIC_ASSERT (last);

		if (last->finished_ok()) {
			message_dialog (this, _("DCPs combined successfully."));
		} else {
			auto m = std_to_wx(last->error_summary());
			if (!last->error_details().empty()) {
				m += wxString::Format(" (%s)", std_to_wx(last->error_details()));
			}
			error_dialog (this, m);
		}
	}

	void setup_sensitivity ()
	{
		_combine->Enable (!_output->GetPath().IsEmpty());
	}

	EditableList<boost::filesystem::path, DirDialogWrapper>* _input;
	wxTextCtrl* _annotation_text = nullptr;
	DirPickerCtrl* _output;
	vector<boost::filesystem::path> _inputs;
	Button* _combine;
};


class App : public wxApp
{
public:
	App () {}

	bool OnInit () override
	{
		try {
			Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			SetAppName (_("DCP-o-matic Combiner"));

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

			_frame = new DOMFrame (_("DCP-o-matic DCP Combiner"));
			SetTopWindow (_frame);

			_frame->Show ();

			signal_manager = new wxSignalManager (this);
			Bind (wxEVT_IDLE, boost::bind(&App::idle, this, _1));
		}
		catch (exception& e)
		{
			error_dialog (nullptr, wxString::Format ("DCP-o-matic DCP Combiner could not start."), std_to_wx(e.what()));
			return false;
		}

		return true;
	}

	void config_failed_to_load ()
	{
		message_dialog (_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx(m));
	}

	void idle (wxIdleEvent& ev)
	{
		signal_manager->ui_idle ();
		ev.Skip ();
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str ())
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format(
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (nullptr, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}
	}

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

	DOMFrame* _frame = nullptr;
};

IMPLEMENT_APP (App)
