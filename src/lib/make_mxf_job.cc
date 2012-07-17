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

/** @file  src/make_mxf_job.cc
 *  @brief A job that creates a MXF file from some data.
 */

#include <iostream>
#include <boost/filesystem.hpp>
#include "AS_DCP.h"
#include "KM_fileio.h"
#include "make_mxf_job.h"
#include "film.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"

using namespace std;
using namespace boost;

/** @class MakeMXFJob
 *  @brief A job that creates a MXF file from some data.
 */

MakeMXFJob::MakeMXFJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l, Type t)
	: Job (s, o, l)
	, _type (t)
{

}

string
MakeMXFJob::name () const
{
	stringstream s;
	switch (_type) {
	case VIDEO:
		s << "Make video MXF for " << _fs->name;
		break;
	case AUDIO:
		s << "Make audio MXF for " << _fs->name;
		break;
	}
	
	return s.str ();
}

void
MakeMXFJob::run ()
{
	set_progress (0);

	string dir;
	switch (_type) {
	case VIDEO:
		dir = _opt->frame_out_path ();
		break;
	case AUDIO:
		dir = _opt->multichannel_audio_out_path ();
		break;
	}

	list<string> files;
        for (filesystem::directory_iterator i = filesystem::directory_iterator (dir); i != filesystem::directory_iterator(); ++i) {
		files.push_back (filesystem::path (*i).string());
	}

	if (files.empty ()) {
		throw EncodeError ("no input files found for MXF");
	}

	files.sort ();

	switch (_type) {
	case VIDEO:
		j2k (files, _fs->file ("video.mxf"));
		break;
	case AUDIO:
		wav (files, _fs->file ("audio.mxf"));
		break;
	default:
		throw EncodeError ("unknown essence type");
	}
	
	set_progress (1);
}

void
MakeMXFJob::wav (list<string> const & files, string const & mxf)
{
	/* XXX: we round for DCP: not sure if this is right */
	ASDCP::Rational fps (rintf (_fs->frames_per_second), 1);
	
 	ASDCP::PCM::WAVParser pcm_parser_channel[files.size()];
	if (pcm_parser_channel[0].OpenRead (files.front().c_str(), fps)) {
		throw EncodeError ("could not open WAV file for reading");
	}
	
	ASDCP::PCM::AudioDescriptor audio_desc;
	pcm_parser_channel[0].FillAudioDescriptor (audio_desc);
	audio_desc.ChannelCount = 0;
	audio_desc.BlockAlign = 0;
	audio_desc.EditRate = fps;
	audio_desc.AvgBps = audio_desc.AvgBps * files.size ();

	ASDCP::PCM::FrameBuffer frame_buffer_channel[files.size()];
	ASDCP::PCM::AudioDescriptor audio_desc_channel[files.size()];
	
	int j = 0;
	for (list<string>::const_iterator i = files.begin(); i != files.end(); ++i) {
		
		if (ASDCP_FAILURE (pcm_parser_channel[j].OpenRead (i->c_str(), fps))) {
			throw EncodeError ("could not open WAV file for reading");
		}

		pcm_parser_channel[j].FillAudioDescriptor (audio_desc_channel[j]);
		frame_buffer_channel[j].Capacity (ASDCP::PCM::CalcFrameBufferSize (audio_desc_channel[j]));

		audio_desc.ChannelCount += audio_desc_channel[j].ChannelCount;
		audio_desc.BlockAlign += audio_desc_channel[j].BlockAlign;
		++j;
	}

	ASDCP::PCM::FrameBuffer frame_buffer;
	frame_buffer.Capacity (ASDCP::PCM::CalcFrameBufferSize (audio_desc));
	frame_buffer.Size (ASDCP::PCM::CalcFrameBufferSize (audio_desc));

	ASDCP::WriterInfo writer_info;
	fill_writer_info (&writer_info);

	ASDCP::PCM::MXFWriter mxf_writer;
	if (ASDCP_FAILURE (mxf_writer.OpenWrite (mxf.c_str(), writer_info, audio_desc))) {
		throw EncodeError ("could not open audio MXF for writing");
	}

	for (int i = 0; i < _fs->length; ++i) {

		byte_t *data_s = frame_buffer.Data();
		byte_t *data_e = data_s + frame_buffer.Capacity();
		byte_t sample_size = ASDCP::PCM::CalcSampleSize (audio_desc_channel[0]);
		int offset = 0;

		for (list<string>::size_type j = 0; j < files.size(); ++j) {
			memset (frame_buffer_channel[j].Data(), 0, frame_buffer_channel[j].Capacity());
			if (ASDCP_FAILURE (pcm_parser_channel[j].ReadFrame (frame_buffer_channel[j]))) {
				throw EncodeError ("could not read audio frame");
			}
			
			if (frame_buffer_channel[j].Size() != frame_buffer_channel[j].Capacity()) {
				throw EncodeError ("short audio frame");
			}
		}

		while (data_s < data_e) {
			for (list<string>::size_type j = 0; j < files.size(); ++j) {
				byte_t* frame = frame_buffer_channel[j].Data() + offset;
				memcpy (data_s, frame, sample_size);
				data_s += sample_size;
			}
			offset += sample_size;
		}

		if (ASDCP_FAILURE (mxf_writer.WriteFrame (frame_buffer, 0, 0))) {
			throw EncodeError ("could not write audio MXF frame");
		}

		set_progress (float (i) / _fs->length);
	}

	if (ASDCP_FAILURE (mxf_writer.Finalize())) {
		throw EncodeError ("could not finalise audio MXF");
	}

	set_progress (1);
	set_state (FINISHED_OK);
}

void
MakeMXFJob::j2k (list<string> const & files, string const & mxf)
{
	ASDCP::JP2K::CodestreamParser j2k_parser;
	ASDCP::JP2K::FrameBuffer frame_buffer (4 * Kumu::Megabyte);
	if (ASDCP_FAILURE (j2k_parser.OpenReadFrame (files.front().c_str(), frame_buffer))) {
		throw EncodeError ("could not open J2K file for reading");
	}
	
	ASDCP::JP2K::PictureDescriptor picture_desc;
	j2k_parser.FillPictureDescriptor (picture_desc);
	/* XXX: we round for DCP: not sure if this is right */
	picture_desc.EditRate = ASDCP::Rational (rintf (_fs->frames_per_second), 1);
	
	ASDCP::WriterInfo writer_info;
	fill_writer_info (&writer_info);
	
	ASDCP::JP2K::MXFWriter mxf_writer;
	if (ASDCP_FAILURE (mxf_writer.OpenWrite (mxf.c_str(), writer_info, picture_desc))) {
		throw EncodeError ("could not open MXF for writing");
	}

	int j = 0;
	for (list<string>::const_iterator i = files.begin(); i != files.end(); ++i) {
		if (ASDCP_FAILURE (j2k_parser.OpenReadFrame (i->c_str(), frame_buffer))) {
			throw EncodeError ("could not open J2K file for reading");
		}

		/* XXX: passing 0 to WriteFrame ok? */
		if (ASDCP_FAILURE (mxf_writer.WriteFrame (frame_buffer, 0, 0))) {
			throw EncodeError ("error in writing video MXF");
		}
		
		++j;
		set_progress (float (j) / files.size ());
	}
	
	if (ASDCP_FAILURE (mxf_writer.Finalize())) {
		throw EncodeError ("error in finalising video MXF");
	}
	
	set_progress (1);
	set_state (FINISHED_OK);
}

void
MakeMXFJob::fill_writer_info (ASDCP::WriterInfo* writer_info)
{
	writer_info->ProductVersion = DVDOMATIC_VERSION;
	writer_info->CompanyName = "dvd-o-matic";
	writer_info->ProductName = "dvd-o-matic";

	/* set the label type */
	writer_info->LabelSetType = ASDCP::LS_MXF_SMPTE;

	/* generate a random UUID for this essence */
	Kumu::GenRandomUUID (writer_info->AssetUUID);
}
