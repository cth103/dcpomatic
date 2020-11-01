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

#include <dcp/array_data.h>

namespace dcpomatic {

dcp::ArrayData random_iv ();
dcp::ArrayData encrypt (std::string plaintext, dcp::ArrayData key, dcp::ArrayData iv);
std::string decrypt (dcp::ArrayData ciphertext, dcp::ArrayData key, dcp::ArrayData iv);
int crypto_key_length ();

}

