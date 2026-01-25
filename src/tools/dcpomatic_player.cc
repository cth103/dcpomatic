/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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

#include "wx/i18n_setup.h"
#include "wx/id.h"
#include "wx/player_frame.h"
#include "wx/wx_ptr.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/dcp_examiner.h"
#include "lib/dcpomatic_socket.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/internet.h"
#include "lib/job.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/ratio.h"
#include "lib/scoped_temporary.h"
#include "lib/server.h"
#include "lib/update_checker.h"
#include "lib/verify_dcp_job.h"
#include <dcp/dcp.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/cmdline.h>
#include <wx/progdlg.h>
#include <wx/splash.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>

#ifdef check
#undef check
#endif


using std::cout;
using std::exception;
using std::list;
using std::string;
using boost::scoped_array;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "config", "Directory containing config.xml", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "s", "stress", "File containing description of stress test", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType(0), 0 }
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App()
		: wxApp()
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads();
#endif
	}

private:

	bool OnInit() override
	{
		wxSplashScreen* splash;
		try {
			wxInitAllImageHandlers();

			Config::FailedToLoad.connect(boost::bind(&App::config_failed_to_load, this));
			Config::Warning.connect(boost::bind(&App::config_warning, this, _1));

			splash = maybe_show_splash();

			SetAppName(variant::wx::dcpomatic_player());

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			make_foreground_application();
#endif

			dcpomatic_setup_path_encoding();

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic::wx::setup_i18n();

			/* Set things up, including filters etc.
			   which will now be internationalised correctly.
			*/
			dcpomatic_setup();

			/* Force the configuration to be re-loaded correctly next
			   time it is needed.
			*/
			Config::drop();

			signal_manager = new wxSignalManager(this);

			_frame = new dcpomatic::ui::PlayerFrame();
			SetTopWindow(_frame);
			_frame->Maximize();
			if (splash) {
				splash->Destroy();
				splash = nullptr;
			}
			_frame->Show();

			if (_dcp_to_load && dcp::filesystem::is_directory(*_dcp_to_load)) {
				try {
					_frame->load_dcp(*_dcp_to_load);
				} catch (exception& e) {
					error_dialog(nullptr, wxString::Format(_("Could not load DCP %s"), std_to_wx(_dcp_to_load->string())), std_to_wx(e.what()));
				}
			}

			if (_stress) {
				try {
					_frame->load_stress_script(*_stress);
				} catch (exception& e) {
					error_dialog(nullptr, wxString::Format(_("Could not load stress test file %s"), std_to_wx(*_stress)));
				}
			}

			Bind(wxEVT_IDLE, boost::bind(&App::idle, this));

			if (Config::instance()->check_for_updates()) {
				UpdateChecker::instance()->run();
			}
		}
		catch (exception& e)
		{
			if (splash) {
				splash->Destroy();
			}
			error_dialog(nullptr, variant::wx::insert_dcpomatic_player(_("%s could not start")), std_to_wx(e.what()));
		}

		return true;
	}

	void OnInitCmdLine(wxCmdLineParser& parser) override
	{
		parser.SetDesc(command_line_description);
		parser.SetSwitchChars(char_to_wx("-"));
	}

	bool OnCmdLineParsed(wxCmdLineParser& parser) override
	{
		if (parser.GetParamCount() > 0) {
			auto path = boost::filesystem::path(wx_to_std(parser.GetParam(0)));
			/* Go at most two directories higher looking for a DCP that contains the file
			 * that was passed in.
			 */
			for (int i = 0; i < 2; ++i) {
				if (dcp::filesystem::is_directory(path) && contains_assetmap(path)) {
					_dcp_to_load = path;
					break;
				}
				path = path.parent_path();
			}
		}

		wxString config;
		if (parser.Found(char_to_wx("c"), &config)) {
			Config::override_path = wx_to_std(config);
		}
		wxString stress;
		if (parser.Found(char_to_wx("s"), &stress)) {
			_stress = wx_to_std(stress);
		}

		return true;
	}

	void report_exception()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog(
				0,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog(
				0,
				wxString::Format(
					_("An exception occurred: %s\n\n%s"),
					std_to_wx(e.what()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), dcpomatic::wx::report_problem()));
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop() override
	{
		report_exception();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException() override
	{
		report_exception();
	}

	void idle()
	{
		signal_manager->ui_idle();
		if (_frame) {
			_frame->idle();
		}
	}

	void config_failed_to_load()
	{
		message_dialog(_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning(string m)
	{
		message_dialog(_frame, std_to_wx(m));
	}

	dcpomatic::ui::PlayerFrame* _frame = nullptr;
	boost::optional<boost::filesystem::path> _dcp_to_load;
	boost::optional<string> _stress;
};

IMPLEMENT_APP(App)
