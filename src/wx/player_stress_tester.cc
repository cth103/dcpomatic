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

#include "player_stress_tester.h"
#include "controls.h"
#include <dcp/raw_convert.h>
#include <dcp/util.h>
#include <wx/wx.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <string>
#include <vector>
#include <iostream>

using std::string;
using std::vector;
using std::cout;
using dcp::raw_convert;
using boost::optional;

/* Interval to check for things to do with the stress script (in milliseconds) */
#define CHECK_INTERVAL 20

Command::Command (string line)
	: type (NONE)
	, int_param (0)
{
	vector<string> bits;
	boost::split (bits, line, boost::is_any_of(" "));
	if (bits[0] == "O") {
		if (bits.size() != 2) {
			return;
		}
		type = OPEN;
		string_param = bits[1];
	} else if (bits[0] == "P") {
		type = PLAY;
	} else if (bits[0] == "W") {
		if (bits.size() != 2) {
			return;
		}
		type = WAIT;
		int_param = raw_convert<int>(bits[1]);
	} else if (bits[0] == "S") {
		type = STOP;
	} else if (bits[0] == "K") {
		if (bits.size() != 2) {
			return;
		}
		type = SEEK;
		int_param = raw_convert<int>(bits[1]);
	}
}

PlayerStressTester::PlayerStressTester ()
	: _parent (0)
	, _controls (0)
	, _suspended (false)
{

}


void
PlayerStressTester::setup (wxWindow* parent, Controls* controls)
{
	_parent = parent;
	_controls = controls;
}


void
PlayerStressTester::load_script (boost::filesystem::path file)
{
	DCPOMATIC_ASSERT (_parent);

	_timer.Bind (wxEVT_TIMER, boost::bind(&PlayerStressTester::check_commands, this));
	_timer.Start (CHECK_INTERVAL);
	vector<string> lines;
	string const script = dcp::file_to_string(file);
	boost::split (lines, script, boost::is_any_of("\n"));
	BOOST_FOREACH (string i, lines) {
		_commands.push_back (Command(i));
	}
	_current_command = _commands.begin();
}

void
PlayerStressTester::check_commands ()
{
	DCPOMATIC_ASSERT (_controls);

	if (_suspended) {
		return;
	}

	if (_current_command == _commands.end()) {
		_timer.Stop ();
		cout << "ST: finished.\n";
		return;
	}

	switch (_current_command->type) {
		case Command::OPEN:
			LoadDCP(_current_command->string_param);
			++_current_command;
			break;
		case Command::PLAY:
			cout << "ST: play\n";
			_controls->play ();
			++_current_command;
			break;
		case Command::WAIT:
			/* int_param here is the number of milliseconds to wait */
			if (_wait_remaining) {
				_wait_remaining = *_wait_remaining - CHECK_INTERVAL;
				if (_wait_remaining < 0) {
					cout << "ST: wait done.\n";
					_wait_remaining = optional<int>();
					++_current_command;
				}
			} else {
				_wait_remaining = _current_command->int_param;
				cout << "ST: waiting for " << *_wait_remaining << ".\n";
			}
			break;
		case Command::STOP:
			cout << "ST: stop\n";
			_controls->stop ();
			++_current_command;
			break;
		case Command::NONE:
			++_current_command;
			break;
		case Command::SEEK:
			/* int_param here is a number between 0 and 4095, corresponding to the possible slider positions */
			cout << "ST: seek to " << _current_command->int_param << "\n";
			_controls->seek (_current_command->int_param);
			++_current_command;
			break;
	}
}

