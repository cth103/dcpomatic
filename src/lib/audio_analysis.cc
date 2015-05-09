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

#include "audio_analysis.h"
#include "dcpomatic_assert.h"
#include "cross.h"
#include <boost/filesystem.hpp>
#include <stdint.h>
#include <inttypes.h>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <iostream>

using std::ostream;
using std::istream;
using std::string;
using std::vector;
using std::cout;
using std::max;
using std::list;

AudioPoint::AudioPoint ()
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = 0;
	}
}

AudioPoint::AudioPoint (FILE* f)
{
	for (int i = 0; i < COUNT; ++i) {
		int n = fscanf (f, "%f", &_data[i]);
		if (n != 1) {
			_data[i] = 0;
		}
	}
}

AudioPoint::AudioPoint (AudioPoint const & other)
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}
}

AudioPoint &
AudioPoint::operator= (AudioPoint const & other)
{
	if (this == &other) {
		return *this;
	}
	
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}

	return *this;
}

void
AudioPoint::write (FILE* f) const
{
	for (int i = 0; i < COUNT; ++i) {
		fprintf (f, "%f\n", _data[i]);
	}
}
	

AudioAnalysis::AudioAnalysis (int channels)
{
	_data.resize (channels);
}

AudioAnalysis::AudioAnalysis (boost::filesystem::path filename)
{
	FILE* f = fopen_boost (filename, "r");
	if (!f) {
		throw OpenFileError (filename);
	}

	int channels = 0;
	fscanf (f, "%d", &channels);
	_data.resize (channels);

	for (int i = 0; i < channels; ++i) {
		int points;
		fscanf (f, "%d", &points);
		if (feof (f)) {
			fclose (f);
			return;
		}
		
		for (int j = 0; j < points; ++j) {
			_data[i].push_back (AudioPoint (f));
			if (feof (f)) {
				fclose (f);
				return;
			}
		}
	}

	/* These may not exist in old analysis files, so be careful
	   about reading them.
	*/
	
	float peak;
	DCPTime::Type peak_time;
	if (fscanf (f, "%f%" SCNd64, &peak, &peak_time) == 2) {
		_peak = peak;
		_peak_time = DCPTime (peak_time);
	}
	
	fclose (f);
}

void
AudioAnalysis::add_point (int c, AudioPoint const & p)
{
	DCPOMATIC_ASSERT (c < channels ());
	_data[c].push_back (p);
}

AudioPoint
AudioAnalysis::get_point (int c, int p) const
{
	DCPOMATIC_ASSERT (p < points (c));
	return _data[c][p];
}

int
AudioAnalysis::channels () const
{
	return _data.size ();
}

int
AudioAnalysis::points (int c) const
{
	DCPOMATIC_ASSERT (c < channels ());
	return _data[c].size ();
}

void
AudioAnalysis::write (boost::filesystem::path filename)
{
	boost::filesystem::path tmp = filename;
	tmp.replace_extension (".tmp");

	FILE* f = fopen_boost (tmp, "w");
	if (!f) {
		throw OpenFileError (tmp);
	}

	fprintf (f, "%ld\n", _data.size ());
	for (vector<vector<AudioPoint> >::iterator i = _data.begin(); i != _data.end(); ++i) {
		fprintf (f, "%ld\n", i->size ());
		for (vector<AudioPoint>::iterator j = i->begin(); j != i->end(); ++j) {
			j->write (f);
		}
	}

	if (_peak) {
		fprintf (f, "%f%" PRId64, _peak.get (), _peak_time.get().get ());
	}

	fclose (f);
	boost::filesystem::rename (tmp, filename);
}
