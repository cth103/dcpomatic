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

#ifdef DCPOMATIC_VARIANT_SWAROOP

#include "lock_file_checker.h"
#include "config.h"
#include "cross.h"

using boost::bind;
using boost::ref;

LockFileChecker* LockFileChecker::_instance = 0;

LockFileChecker::LockFileChecker ()
	: Checker (10)
{

}

bool
LockFileChecker::check () const
{
	return !Config::instance()->player_lock_file() || boost::filesystem::is_regular_file(Config::instance()->player_lock_file().get());
}

LockFileChecker *
LockFileChecker::instance ()
{
	if (!_instance) {
		_instance = new LockFileChecker ();
	}

	return _instance;
}

#endif
