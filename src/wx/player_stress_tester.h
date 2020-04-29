/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>

class wxWindow;
class Controls;

class Command
{
public:
	enum Type {
		NONE,
		OPEN,
		PLAY,
		WAIT,
		STOP,
		SEEK,
		EXIT
	};

	Command(std::string line);

	Type type;
	std::string string_param;
	int int_param;
};

class PlayerStressTester
{
public:
	PlayerStressTester ();

	void setup (wxWindow* parent, Controls* controls);
	void load_script (boost::filesystem::path file);
	void set_suspended (bool s) {
		_suspended = s;
	}

	boost::signals2::signal<void (boost::filesystem::path)> LoadDCP;

private:
	void check_commands ();

	wxWindow* _parent;
	Controls* _controls;
	wxTimer _timer;
	bool _suspended;
	std::list<Command> _commands;
	std::list<Command>::const_iterator _current_command;
	/** Remaining time that the script must wait, in milliseconds */
	boost::optional<int> _wait_remaining;
};

