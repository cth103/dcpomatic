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


#include "signaller.h"
#include "state.h"
#include <boost/signals2.hpp>


class Analytics : public State, public Signaller
{
public:
	Analytics() = default;

	void successful_dcp_encode ();

	void write () const override;
	void read () override;

	boost::signals2::signal<void (std::string, std::string)> Message;

	static Analytics* instance ();

private:
	int _successful_dcp_encodes = 0;

	static Analytics* _instance;
	static int const _current_version;
};
