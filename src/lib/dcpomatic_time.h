/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/dcpomatic_time.h
 *  @brief Types to describe time.
 */

#ifndef DCPOMATIC_TIME_H
#define DCPOMATIC_TIME_H

#include "frame_rate_change.h"
#include "dcpomatic_assert.h"
#include <boost/optional.hpp>
#include <stdint.h>
#include <cmath>
#include <ostream>
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
	Time<S, O> round_up (float r) const {
		Type const n = llrintf (HZ / r);
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
	int64_t frames_round (T r) const {
		/* We must cast to double here otherwise if T is integer
		   the calculation will round down before we get the chance
		   to llrint().
		*/
		return llrint (_t * double(r) / HZ);
	}

	template <typename T>
	int64_t frames_floor (T r) const {
		return floor (_t * r / HZ);
	}

	template <typename T>
	int64_t frames_ceil (T r) const {
		/* We must cast to double here otherwise if T is integer
		   the calculation will round down before we get the chance
		   to ceil().
		*/
		return ceil (_t * double(r) / HZ);
	}

	/** @param r Frames per second */
	template <typename T>
	void split (T r, int& h, int& m, int& s, int& f) const
	{
		/* Do this calculation with frames so that we can round
		   to a frame boundary at the start rather than the end.
		*/
		int64_t ff = frames_round (r);

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

		char buffer[128];
		snprintf (buffer, sizeof (buffer), "%02d:%02d:%02d:%02d", h, m, s, f);
		return buffer;
	}


	static Time<S, O> from_seconds (double s) {
		return Time<S, O> (llrint (s * HZ));
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

template <class T>
class TimePeriod
{
public:
	TimePeriod () {}

	TimePeriod (T f, T t)
		: from (f)
		, to (t)
	{}

	/** start time of sampling interval that the period is from */
	T from;
	/** start time of next sampling interval after the period */
	T to;

	T duration () const {
		return to - from;
	}

	TimePeriod<T> operator+ (T const & o) const {
		return TimePeriod<T> (from + o, to + o);
	}

	boost::optional<TimePeriod<T> > overlap (TimePeriod<T> const & other) {
		T const max_from = std::max (from, other.from);
		T const min_to = std::min (to, other.to);

		if (max_from >= min_to) {
			return boost::optional<TimePeriod<T> > ();
		}

		return TimePeriod<T> (max_from, min_to);
	}

	bool contains (T const & other) const {
		return (from <= other && other < to);
	}

	bool operator== (TimePeriod<T> const & other) const {
		return from == other.from && to == other.to;
	}
};

typedef TimePeriod<ContentTime> ContentTimePeriod;
typedef TimePeriod<DCPTime> DCPTimePeriod;

DCPTime min (DCPTime a, DCPTime b);
DCPTime max (DCPTime a, DCPTime b);
ContentTime min (ContentTime a, ContentTime b);
ContentTime max (ContentTime a, ContentTime b);
std::string to_string (ContentTime t);
std::string to_string (DCPTime t);
std::string to_string (DCPTimePeriod p);

#endif
