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

#ifndef DVDOMATIC_AUDIO_ANALYSIS_H
#define DVDOMATIC_AUDIO_ANALYSIS_H

#include <iostream>
#include <vector>
#include <list>

class AudioPoint
{
public:
	enum Type {
		PEAK,
		RMS,
		COUNT
	};

	AudioPoint ();
	AudioPoint (std::istream &);

	void write (std::ostream &) const;
	
	float& operator[] (int t) {
		return _data[t];
	}

private:
	float _data[COUNT];
};

class AudioAnalysis
{
public:
	AudioAnalysis (int c);
	AudioAnalysis (std::string);

	void add_point (int c, AudioPoint const & p);
	
	AudioPoint get_point (int c, int p) const;
	int points (int c) const;
	int channels () const;

	void write (std::string);

private:
	std::vector<std::vector<AudioPoint> > _data;
};

#endif
