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

#ifndef DCPOMATIC_TIME_H
#define DCPOMATIC_TIME_H

#include "frame_rate_change.h"
#include "safe_stringstream.h"
#include "dcpomatic_assert.h"
#include <stdint.h>
#include <cmath>
#include <ostream>
#include <sstream>
#include <iomanip>

class dcpomatic_round_up_test;

class Time;

/** A time in seconds, expressed as a number scaled up by Time::HZ. */
class Time
{
public:
	Time ()
		: _t (0)
	{}

	typedef int64_t Type;

	explicit Time (Type t)
		: _t (t)
	{}

	virtual ~Time () {}

	Type get () const {
		return _t;
	}

	double seconds () const {
		return double (_t) / HZ;
	}

	template <typename T>
	int64_t frames (T r) const {
		return rint (double (_t) * r / HZ);
	}

	/** @param r Frames per second */
	template <typename T>
	void split (T r, int& h, int& m, int& s, int& f) const
	{
		/* Do this calculation with frames so that we can round
		   to a frame boundary at the start rather than the end.
		*/
		int64_t ff = frames (r);
		
		h = ff / (3600 * r);
		ff -= h * 3600 * r;
		m = ff / (60 * r);
		ff -= m * 60 * r;
		s = ff / r;
		ff -= s * r;

		f = static_cast<int> (ff);
	}

	template <typename T>
	std::string timecode (T r) const {
		int h;
		int m;
		int s;
		int f;
		split (r, h, m, s, f);

		SafeStringStream o;
		o.width (2);
		o.fill ('0');
		o << std::setw(2) << std::setfill('0') << h << ":"
		  << std::setw(2) << std::setfill('0') << m << ":"
		  << std::setw(2) << std::setfill('0') << s << ":"
		  << std::setw(2) << std::setfill('0') << f;
		return o.str ();
	}

protected:
	friend struct dcptime_round_up_test;
	
	Type _t;
	static const int HZ = 96000;
};

class DCPTime;

class ContentTime : public Time
{
public:
	ContentTime () : Time () {}
	explicit ContentTime (Type t) : Time (t) {}
	ContentTime (Type n, Type d) : Time (n * HZ / d) {}
	ContentTime (DCPTime d, FrameRateChange f);

	bool operator< (ContentTime const & o) const {
		return _t < o._t;
	}

	bool operator<= (ContentTime const & o) const {
		return _t <= o._t;
	}

	bool operator== (ContentTime const & o) const {
		return _t == o._t;
	}

	bool operator!= (ContentTime const & o) const {
		return _t != o._t;
	}

	bool operator>= (ContentTime const & o) const {
		return _t >= o._t;
	}

	bool operator> (ContentTime const & o) const {
		return _t > o._t;
	}

	ContentTime operator+ (ContentTime const & o) const {
		return ContentTime (_t + o._t);
	}

	ContentTime & operator+= (ContentTime const & o) {
		_t += o._t;
		return *this;
	}

	ContentTime operator- () const {
		return ContentTime (-_t);
	}

	ContentTime operator- (ContentTime const & o) const {
		return ContentTime (_t - o._t);
	}

	ContentTime & operator-= (ContentTime const & o) {
		_t -= o._t;
		return *this;
	}

	/** Round up to the nearest sampling interval
	 *  at some sampling rate.
	 *  @param r Sampling rate.
	 */
	ContentTime round_up (float r) {
		Type const n = rint (HZ / r);
		Type const a = _t + n - 1;
		return ContentTime (a - (a % n));
	}

	static ContentTime from_seconds (double s) {
		return ContentTime (s * HZ);
	}

	template <class T>
	static ContentTime from_frames (int64_t f, T r) {
		DCPOMATIC_ASSERT (r > 0);
		return ContentTime (f * HZ / r);
	}

	static ContentTime max () {
		return ContentTime (INT64_MAX);
	}
};

std::ostream& operator<< (std::ostream& s, ContentTime t);

class ContentTimePeriod
{
public:
	ContentTimePeriod () {}
	ContentTimePeriod (ContentTime f, ContentTime t)
		: from (f)
		, to (t)
	{}

	ContentTime from;
	ContentTime to;

	ContentTimePeriod operator+ (ContentTime const & o) const {
		return ContentTimePeriod (from + o, to + o);
	}

	bool overlaps (ContentTimePeriod const & o) const;
	bool contains (ContentTime const & o) const;
};

class DCPTime : public Time
{
public:
	DCPTime () : Time () {}
	explicit DCPTime (Type t) : Time (t) {}
	DCPTime (ContentTime t, FrameRateChange c) : Time (rint (t.get() / c.speed_up)) {}

	bool operator< (DCPTime const & o) const {
		return _t < o._t;
	}

	bool operator<= (DCPTime const & o) const {
		return _t <= o._t;
	}

	bool operator== (DCPTime const & o) const {
		return _t == o._t;
	}

	bool operator!= (DCPTime const & o) const {
		return _t != o._t;
	}

	bool operator>= (DCPTime const & o) const {
		return _t >= o._t;
	}

	bool operator> (DCPTime const & o) const {
		return _t > o._t;
	}

	DCPTime operator+ (DCPTime const & o) const {
		return DCPTime (_t + o._t);
	}

	DCPTime & operator+= (DCPTime const & o) {
		_t += o._t;
		return *this;
	}

	DCPTime operator- () const {
		return DCPTime (-_t);
	}

	DCPTime operator- (DCPTime const & o) const {
		return DCPTime (_t - o._t);
	}

	DCPTime & operator-= (DCPTime const & o) {
		_t -= o._t;
		return *this;
	}

	/** Round up to the nearest sampling interval
	 *  at some sampling rate.
	 *  @param r Sampling rate.
	 */
	DCPTime round_up (float r) {
		Type const n = rint (HZ / r);
		Type const a = _t + n - 1;
		return DCPTime (a - (a % n));
	}

	DCPTime abs () const {
		return DCPTime (std::abs (_t));
	}

	static DCPTime from_seconds (double s) {
		return DCPTime (s * HZ);
	}

	template <class T>
	static DCPTime from_frames (int64_t f, T r) {
		DCPOMATIC_ASSERT (r > 0);
		return DCPTime (f * HZ / r);
	}

	static DCPTime delta () {
		return DCPTime (1);
	}

	static DCPTime max () {
		return DCPTime (INT64_MAX);
	}
};

DCPTime min (DCPTime a, DCPTime b);
std::ostream& operator<< (std::ostream& s, DCPTime t);

#endif
