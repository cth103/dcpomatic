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

#include <cmath>
#include <ostream>
#include <stdint.h>
#include "frame_rate_change.h"

class dcpomatic_round_up_test;

class Time;

/** A time in seconds, expressed as a number scaled up by Time::HZ. */
class Time
{
public:
	Time ()
		: _t (0)
	{}

	explicit Time (int64_t t)
		: _t (t)
	{}

	virtual ~Time () {}

	int64_t get () const {
		return _t;
	}

	double seconds () const {
		return double (_t) / HZ;
	}

	template <typename T>
	int64_t frames (T r) const {
		return rint (_t * r / HZ);
	}

protected:
	friend class dcptime_round_up_test;
	
	int64_t _t;
	static const int HZ = 96000;
};

class DCPTime;

class ContentTime : public Time
{
public:
	ContentTime () : Time () {}
	explicit ContentTime (int64_t t) : Time (t) {}
	ContentTime (int64_t n, int64_t d) : Time (n * HZ / d) {}
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
	ContentTime round_up (int r) {
		int64_t const n = HZ / r;
		int64_t const a = _t + n - 1;
		return ContentTime (a - (a % n));
	}
	

	static ContentTime from_seconds (double s) {
		return ContentTime (s * HZ);
	}

	template <class T>
	static ContentTime from_frames (int64_t f, T r) {
		return ContentTime (f * HZ / r);
	}

	static ContentTime max () {
		return ContentTime (INT64_MAX);
	}
};

std::ostream& operator<< (std::ostream& s, ContentTime t);

class DCPTime : public Time
{
public:
	DCPTime () : Time () {}
	explicit DCPTime (int64_t t) : Time (t) {}
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
	DCPTime round_up (int r) {
		int64_t const n = HZ / r;
		int64_t const a = _t + n - 1;
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
