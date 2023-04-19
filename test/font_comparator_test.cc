#include "lib/font.h"
#include "lib/font_comparator.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;


BOOST_AUTO_TEST_CASE(font_comparator_test)
{
	map<dcpomatic::Font::Content, string, FontComparator> cache;

	auto font = make_shared<dcpomatic::Font>("foo");

	BOOST_CHECK(cache.find(font->content()) == cache.end());
	cache[font->content()] = "foo";
	BOOST_CHECK(cache.find(font->content()) != cache.end());

	font->set_file("test/data/Inconsolata-VF.ttf");
	BOOST_CHECK(cache.find(font->content()) == cache.end());
}


