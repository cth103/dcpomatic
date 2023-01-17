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


#include "collator.h"
#include "dcpomatic_assert.h"
#include <unicode/putil.h>
#include <unicode/ucol.h>
#include <unicode/uiter.h>
#include <unicode/utypes.h>
#include <unicode/ustring.h>
#include <boost/scoped_array.hpp>
#include <cstring>
#include <vector>


using std::string;
using std::vector;


Collator::Collator()
{
	UErrorCode status = U_ZERO_ERROR;
	_collator = ucol_open(nullptr, &status);
	if (_collator) {
		ucol_setAttribute(_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
		ucol_setAttribute(_collator, UCOL_STRENGTH, UCOL_PRIMARY, &status);
		ucol_setAttribute(_collator, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
	}
}


Collator::~Collator()
{
	if (_collator) {
		ucol_close (_collator);
	}
}


vector<UChar>
utf8_to_utf16(string const& utf8)
{
	vector<UChar> utf16(utf8.size() + 1);
	UErrorCode error = U_ZERO_ERROR;
	u_strFromUTF8(utf16.data(), utf8.size() + 1, nullptr, utf8.c_str(), -1, &error);
	DCPOMATIC_ASSERT(error == U_ZERO_ERROR);
	return utf16;
}


int
Collator::compare (string const& utf8_a, string const& utf8_b) const
{
	if (_collator) {
		auto utf16_a = utf8_to_utf16(utf8_a);
		auto utf16_b = utf8_to_utf16(utf8_b);
		return ucol_strcoll(_collator, utf16_a.data(), -1, utf16_b.data(), -1);
	} else {
		return strcoll(utf8_a.c_str(), utf8_b.c_str());
	}
}

