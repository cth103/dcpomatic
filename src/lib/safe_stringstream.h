/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef DCPOMATIC_SAFE_STRINGSTREAM_H
#define DCPOMATIC_SAFE_STRINGSTREAM_H

#include <boost/thread/mutex.hpp>

/* I've not been able to reproduce it, but there have been reports that DCP-o-matic crashes
 * on OS X with two simultaneous backtraces that look like this:
 *
 * 0 libSystem.B.dylib  0x00007fff84ebe264 __numeric_load_locale + 125
 * 1 libSystem.B.dylib  0x00007fff84e2aac4 loadlocale + 323
 * 2 libstdc++.6.dylib  0x00007fff8976ba69 std::__convert_from_v(int* const&, char*, int, char const*, ...) + 199
 * 3 libstdc++.6.dylib  0x00007fff8974e99b std::ostreambuf_iterator<char, std::char_traits<char> > std::num_put<char, std::ostreambuf_iterator<char,
std::char_traits<char> > >::_M_insert_float<double>(std::ostreambuf_iterator<char, std::char_traits<char> >, std::ios_base&, char, char, double) const + 199
 * 4 libstdc++.6.dylib  0x00007fff8974ebc0 std::num_put<char, std::ostreambuf_iterator<char, std::char_traits<char> >
>::do_put(std::ostreambuf_iterator<char, std::char_traits<char> >, std::ios_base&, char, double) const + 28
 * 5 libstdc++.6.dylib  0x00007fff897566a2 std::ostream& std::ostream::_M_insert<double>(double) + 178
 * 6 libdcpomatic.dylib 0x0000000100331e21 StringPrivate::Composition& StringPrivate::Composition::arg<float>(float const&) + 33
 *
 * in two different threads.  I'm assuming that for some bizarre reason it is unsafe to use two separate stringstream
 * objects in different threads on OS X.  This is a hack to work around it.
 */

class SafeStringStream
{
public:
	SafeStringStream ()
	{}
	
	SafeStringStream (std::string s)
		: _stream (s)
	{}
	
	template <class T>
	std::ostream& operator<< (T val)
	{
		boost::mutex::scoped_lock lm (_mutex);
		_stream << val;
		return _stream;
	}

	template <class T>
	std::istream& operator>> (T& val)
	{
		boost::mutex::scoped_lock lm (_mutex);
		_stream >> val;
		return _stream;
	}

	std::string str () const {
		return _stream.str ();
	}

	void str (std::string const & s) {
		_stream.str (s);
	}

	void imbue (std::locale const & loc)
	{
		boost::mutex::scoped_lock lm (_mutex);
		_stream.imbue (loc);
	}

	void width (int w)
	{
		_stream.width (w);
	}

	void fill (int f)
	{
		_stream.fill (f);
	}
	
	void precision (int p)
	{
		_stream.precision (p);
	}

	bool good () const
	{
		return _stream.good ();
	}

	std::string getline ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		std::string s;
		std::getline (_stream, s);
		return s;
	}

	void setf (std::ios_base::fmtflags flags, std::ios_base::fmtflags mask)
	{
		_stream.setf (flags, mask);
	}

private:
	static boost::mutex _mutex;
	std::stringstream _stream;
};

#endif
