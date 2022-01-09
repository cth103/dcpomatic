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


#include "rng.h"
#include <stdint.h>


using namespace dcpomatic;


RNG::RNG (int32_t seed)
	: _state (static_cast<uint32_t>(seed))
{

}


int32_t
RNG::get ()
{
	uint32_t const b = ((_state >> 0) ^ (_state >> 1) ^ (_state >> 2) ^ (_state >> 7));
	_state = (_state >> 1) | (b << 23);
	return static_cast<int32_t>(_state);
}

