/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <stdint.h>
#include <cmath>
#include <cassert>
#include <fstream>
#include <boost/filesystem.hpp>
#include "audio_analysis.h"

using std::ostream;
using std::istream;
using std::string;
using std::ofstream;
using std::ifstream;
using std::vector;
using std::cout;

AudioPoint::AudioPoint ()
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = 0;
	}
}

AudioPoint::AudioPoint (istream& s)
{
	for (int i = 0; i < COUNT; ++i) {
		s >> _data[i];
	}
}

void
AudioPoint::write (ostream& s) const
{
	for (int i = 0; i < COUNT; ++i) {
		s << _data[i] << "\n";
	}
}
	

AudioAnalysis::AudioAnalysis (int channels)
{
	_data.resize (channels);
}

AudioAnalysis::AudioAnalysis (string filename)
{
	ifstream f (filename.c_str ());

	int channels;
	f >> channels;
	_data.resize (channels);

	for (int i = 0; i < channels; ++i) {
		int points;
		f >> points;
		for (int j = 0; j < points; ++j) {
			_data[i].push_back (AudioPoint (f));
		}
	}
}

void
AudioAnalysis::add_point (int c, AudioPoint const & p)
{
	assert (c < int (_data.size ()));
	_data[c].push_back (p);
}

AudioPoint
AudioAnalysis::get_point (int c, int p) const
{
	assert (c < int (_data.size ()));
	assert (p < int (_data[c].size ()));
	return _data[c][p];
}

int
AudioAnalysis::points (int c) const
{
	assert (c < int (_data.size ()));
	return _data[c].size ();
}

void
AudioAnalysis::write (string filename)
{
	string tmp = filename + ".tmp";
	
	ofstream f (tmp.c_str ());
	f << _data.size() << "\n";
	for (vector<vector<AudioPoint> >::iterator i = _data.begin(); i != _data.end(); ++i) {
		f << i->size () << "\n";
		for (vector<AudioPoint>::iterator j = i->begin(); j != i->end(); ++j) {
			j->write (f);
		}
	}

	f.close ();
	boost::filesystem::rename (tmp, filename);
}
