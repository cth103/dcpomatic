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
#include <unicode/putil.h>
#include <unicode/ucol.h>
#include <unicode/uiter.h>
#include <unicode/utypes.h>
#include <unicode/ustring.h>
#include <boost/scoped_array.hpp>
#include <cstring>


using std::string;


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


int
Collator::compare (string const& utf8_a, string const& utf8_b) const
{
	if (_collator) {
		UErrorCode error = U_ZERO_ERROR;
		boost::scoped_array<uint16_t> utf16_a(new uint16_t[utf8_a.size() + 1]);
		u_strFromUTF8(reinterpret_cast<UChar*>(utf16_a.get()), utf8_a.size() + 1, nullptr, utf8_a.c_str(), -1, &error);
		boost::scoped_array<uint16_t> utf16_b(new uint16_t[utf8_b.size() + 1]);
		u_strFromUTF8(reinterpret_cast<UChar*>(utf16_b.get()), utf8_b.size() + 1, nullptr, utf8_b.c_str(), -1, &error);
		return ucol_strcoll(_collator, reinterpret_cast<UChar*>(utf16_a.get()), -1, reinterpret_cast<UChar*>(utf16_b.get()), -1);
	} else {
		return strcoll(utf8_a.c_str(), utf8_b.c_str());
	}
}

