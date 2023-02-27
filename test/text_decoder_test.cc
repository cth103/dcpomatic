/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/text_decoder.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(strip_invalid_characters_for_xml_test)
{
	BOOST_CHECK_EQUAL(TextDecoder::remove_invalid_characters_for_xml("hello world"), "hello world");
	BOOST_CHECK_EQUAL(TextDecoder::remove_invalid_characters_for_xml("hello\x0cworld"), "helloworld");
	BOOST_CHECK_EQUAL(TextDecoder::remove_invalid_characters_for_xml("𒀖hello\x02worl𒁝d"), "𒀖helloworl𒁝d");
	BOOST_CHECK_EQUAL(TextDecoder::remove_invalid_characters_for_xml("😀œ´®†¥¨ˆø\x09π¬˚∆\x1a˙©ƒ∂ßåΩ≈ç√∫\x02˜µ≤ユーザーコードa"), "😀œ´®†¥¨ˆø\x09π¬˚∆˙©ƒ∂ßåΩ≈ç√∫˜µ≤ユーザーコードa");
}
