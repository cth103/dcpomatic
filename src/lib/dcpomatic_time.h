/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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
#include <cstdio>

struct dcpomatic_time_ceil_test;
struct dcpomatic_time_floor_test;

namespace dcpomatic {


class HMSF
{
public:
	HMSF () {}

	HMSF (int h_, int m_, int s_, int f_)
		: h(h_)
		, m(m_)
		, s(s_)
		, f(f_)
	{}

	int h = 0;
	int m = 0;
	int s = 0;
	int f = 0;
};


/** A time in seconds, expressed as a number scaled up by Time::HZ.  We want two different
 *  versions of this class, dcpomatic::ContentTime and dcpomatic::DCPTime, and we want it to be impossible to
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

	/** @param hmsf Hours, minutes, seconds, frames.
	 *  @param fps Frame rate
	 */
	Time (HMSF const& hmsf, float fps) {
		*this = from_seconds (hmsf.h * 3600)
			+ from_seconds (hmsf.m * 60)
			+ from_seconds (hmsf.s)
			+ from_frames (hmsf.f, fps);
	}

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

	Time<S, O> operator/ (int o) const {
		return Time<S, O> (_t / o);
	}

	/** Round up to the nearest sampling interval
	 *  at some sampling rate.
	 *  @param r Sampling rate.
	 */
	Time<S, O> ceil (double r) const {
		return Time<S, O> (llrint (HZ * frames_ceil(r) / r));
	}

	Time<S, O> floor (double r) const {
		return Time<S, O> (llrint (HZ * frames_floor(r) / r));
	}

	Time<S, O> round (double r) const {
		return Time<S, O> (llrint (HZ * frames_round(r) / r));
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
		return ::floor (_t * r / HZ);
	}

	template <typename T>
	int64_t frames_ceil (T r) const {
		/* We must cast to double here otherwise if T is integer
		   the calculation will round down before we get the chance
		   to ceil().
		*/
		return ::ceil (_t * double(r) / HZ);
	}

	/** Split a time into hours, minutes, seconds and frames.
	 *  @param r Frames per second.
	 *  @return Split time.
	 */
	template <typename T>
	HMSF split (T r) const
	{
		/* Do this calculation with frames so that we can round
		   to a frame boundary at the start rather than the end.
		*/
		auto ff = frames_round (r);
		HMSF hmsf;

		hmsf.h = ff / (3600 * r);
		ff -= hmsf.h * 3600 * r;
		hmsf.m = ff / (60 * r);
		ff -= hmsf.m * 60 * r;
		hmsf.s = ff / r;
		ff -= hmsf.s * r;

		hmsf.f = static_cast<int> (ff);
		return hmsf;
	}

	template <typename T>
	std::string timecode (T r) const {
		auto hmsf = split (r);

		char buffer[128];
		snprintf (buffer, sizeof(buffer), "%02d:%02d:%02d:%02d", hmsf.h, hmsf.m, hmsf.s, hmsf.f);
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

	static const int HZ = 96000;

private:
	friend struct ::dcpomatic_time_ceil_test;
	friend struct ::dcpomatic_time_floor_test;

	Type _t;
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

	boost::optional<TimePeriod<T> > overlap (TimePeriod<T> const & other) const {
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

	bool operator< (TimePeriod<T> const & o) const {
		if (from != o.from) {
			return from < o.from;
		}
		return to < o.to;
	}

	bool operator== (TimePeriod<T> const & other) const {
		return from == other.from && to == other.to;
	}

	bool operator!= (TimePeriod<T> const & other) const {
		return !(*this == other);
	}
};

/** @param A Period which is subtracted from.
 *  @param B Periods to subtract from `A', must be in ascending order of start time and must not overlap.
 */
template <class T>
std::list<TimePeriod<T> > subtract (TimePeriod<T> A, std::list<TimePeriod<T> > const & B)
{
	std::list<TimePeriod<T> > result;
	result.push_back (A);

	for (auto i: B) {
		std::list<TimePeriod<T> > new_result;
		for (auto j: result) {
			boost::optional<TimePeriod<T> > ov = i.overlap (j);
			if (ov) {
				if (*ov == i) {
					/* A contains all of B */
					if (i.from != j.from) {
						new_result.push_back (TimePeriod<T> (j.from, i.from));
					}
					if (i.to != j.to) {
						new_result.push_back (TimePeriod<T> (i.to, j.to));
					}
				} else if (*ov == j) {
					/* B contains all of A */
				} else if (i.from < j.from) {
					/* B overlaps start of A */
					new_result.push_back (TimePeriod<T> (i.to, j.to));
				} else if (i.to > j.to) {
					/* B overlaps end of A */
					new_result.push_back (TimePeriod<T> (j.from, i.from));
				}
			} else {
				new_result.push_back (j);
			}
		}
		result = new_result;
	}

	return result;
}

typedef TimePeriod<ContentTime> ContentTimePeriod;
typedef TimePeriod<DCPTime> DCPTimePeriod;

DCPTime min (DCPTime a, DCPTime b);
DCPTime max (DCPTime a, DCPTime b);
ContentTime min (ContentTime a, ContentTime b);
ContentTime max (ContentTime a, ContentTime b);
std::string to_string (ContentTime t);
std::string to_string (DCPTime t);
std::string to_string (DCPTimePeriod p);

}

#endif
