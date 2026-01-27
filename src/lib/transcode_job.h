/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_TRANSCODE_JOB_H
#define DCPOMATIC_TRANSCODE_JOB_H


/** @file src/transcode_job.h
 *  @brief A job which transcodes from one format to another.
 */


#include "job.h"


/* Defined by Windows */
#undef IGNORE


class FilmEncoder;

struct frames_not_lost_when_threads_disappear;


/** @class TranscodeJob
 *  @brief A job which transcodes a Film to another format.
 */
class TranscodeJob : public Job
{
public:
	enum class ChangedBehaviour {
		EXAMINE_THEN_STOP,
		STOP,
		IGNORE
	};

	TranscodeJob(std::shared_ptr<const Film> film, ChangedBehaviour changed);
	~TranscodeJob();

	std::string name() const override;
	std::string json_name() const override;
	void run() override;
	void pause() override;
	void resume() override;
	std::string status() const override;
	bool enable_notify() const override {
		return true;
	}

	void set_encoder(std::shared_ptr<FilmEncoder> encoder);

private:
	friend struct ::frames_not_lost_when_threads_disappear;

	virtual void post_transcode() {}
	float frames_per_second() const;

	int remaining_time() const override;

	std::shared_ptr<FilmEncoder> _encoder;
	ChangedBehaviour _changed;
};


#endif

