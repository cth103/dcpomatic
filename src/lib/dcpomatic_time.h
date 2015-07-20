/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/dcpomatic_time.h
 *  @brief Types to describe time.
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

/** A time in seconds, expressed as a number scaled up by Time::HZ.  We want two different
 *  versions of this class, ContentTime and DCPTime, and we want it to be impossible to
 *  convert implicitly between the two.  Hence there's this template hack.  I'm not
 *  sure if it's the best way to do it.
 *
 *  S is the name of `this' class and O is its opposite (see the typedefs below).
 */
template <class S, class O>
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

	explicit Time (Type n, Type d)
		: _t (n * HZ / d)
	{}

	/* Explicit conversion from type O */
	Time (Time<O, S> d, FrameRateChange f);

	Type get () const {
		return _t;
	}

	bool operator< (Time<S, O> const & o) const {
		return _t < o._t;
	}

	bool operator<= (Time<S, O> const & o) const {
		return _t <= o._t;
	}

	bool operator== (Time<S, O> const & o) const {
		return _t == o._t;
	}

	bool operator!= (Time<S, O> const & o) const {
		return _t != o._t;
	}

	bool operator>= (Time<S, O> const & o) const {
		return _t >= o._t;
	}

	bool operator> (Time<S, O> const & o) const {
		return _t > o._t;
	}

	Time<S, O> operator+ (Time<S, O> const & o) const {
		return Time<S, O> (_t + o._t);
	}

	Time<S, O> & operator+= (Time<S, O> const & o) {
		_t += o._t;
		return *this;
	}

	Time<S, O> operator- () const {
		return Time<S, O> (-_t);
	}

	Time<S, O> operator- (Time<S, O> const & o) const {
		return Time<S, O> (_t - o._t);
	}

	Time<S, O> & operator-= (Time<S, O> const & o) {
		_t -= o._t;
		return *this;
	}

	/** Round up to the nearest sampling interval
	 *  at some sampling rate.
	 *  @param r Sampling rate.
	 */
	Time<S, O> round_up (float r) {
		Type const n = rint (HZ / r);
		Type const a = _t + n - 1;
		return Time<S, O> (a - (a % n));
	}

	double seconds () const {
		return double (_t) / HZ;
	}

	Time<S, O> abs () const {
		return Time<S, O> (std::abs (_t));
	}

	template <typename T>
	int64_t frames (T r) const {
		return floor (_t * r / HZ);
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


	static Time<S, O> from_seconds (double s) {
		return Time<S, O> (rint (s * HZ));
	}

	template <class T>
	static Time<S, O> from_frames (int64_t f, T r) {
		DCPOMATIC_ASSERT (r > 0);
		return Time<S, O> (f * HZ / r);
	}

	static Time<S, O> delta () {
		return Time<S, O> (1);
	}

	static Time<S, O> min () {
		return Time<S, O> (-INT64_MAX);
	}

	static Time<S, O> max () {
		return Time<S, O> (INT64_MAX);
	}

private:
	friend struct dcptime_round_up_test;

	Type _t;
	static const int HZ = 96000;
};

class ContentTimeDifferentiator {};
class DCPTimeDifferentiator {};

/* Specializations for the two allowed explicit conversions */

template<>
Time<ContentTimeDifferentiator, DCPTimeDifferentiator>::Time (Time<DCPTimeDifferentiator, ContentTimeDifferentiator> d, FrameRateChange f);

template<>
Time<DCPTimeDifferentiator, ContentTimeDifferentiator>::Time (Time<ContentTimeDifferentiator, DCPTimeDifferentiator> d, FrameRateChange f);

/** Time relative to the start or position of a piece of content in its native frame rate */
typedef Time<ContentTimeDifferentiator, DCPTimeDifferentiator> ContentTime;
/** Time relative to the start of the output DCP in its frame rate */
typedef Time<DCPTimeDifferentiator, ContentTimeDifferentiator> DCPTime;

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

	bool operator== (ContentTimePeriod const & o) const {
		return from == o.from && to == o.to;
	}
};

DCPTime min (DCPTime a, DCPTime b);
DCPTime max (DCPTime a, DCPTime b);
ContentTime min (ContentTime a, ContentTime b);
ContentTime max (ContentTime a, ContentTime b);
std::ostream& operator<< (std::ostream& s, ContentTime t);
std::ostream& operator<< (std::ostream& s, DCPTime t);

#endif
