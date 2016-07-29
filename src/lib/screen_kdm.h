/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SCREEN_KDM_H
#define DCPOMATIC_SCREEN_KDM_H

#include "kdm_name_format.h"
#include <dcp/encrypted_kdm.h>
#include <boost/shared_ptr.hpp>

class Screen;

/** Simple class to collect a screen and an encrypted KDM */
class ScreenKDM
{
public:
	ScreenKDM (boost::shared_ptr<Screen> s, dcp::EncryptedKDM k)
		: screen (s)
		, kdm (k)
	{}

	static void write_files (
		std::list<ScreenKDM> screen_kdms, boost::filesystem::path directory,
		KDMNameFormat name_format, dcp::NameFormat::Map name_values
		);

	boost::shared_ptr<Screen> screen;
	dcp::EncryptedKDM kdm;
};

extern bool operator== (ScreenKDM const & a, ScreenKDM const & b);

#endif
