/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "examine_ffmpeg_subtitles_job.h"
#include "ffmpeg_content.h"
#include "ffmpeg_subtitle_stream.h"
#include "text_content.h"
#include "cross.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ExamineFFmpegSubtitlesJob::ExamineFFmpegSubtitlesJob (shared_ptr<const Film> film, shared_ptr<FFmpegContent> c)
	: Job (film)
	, FFmpeg (c)
	, _content (c)
{

}

string
ExamineFFmpegSubtitlesJob::name () const
{
	return _("Examining subtitles");
}

string
ExamineFFmpegSubtitlesJob::json_name () const
{
	return N_("examine_subtitles");
}

void
ExamineFFmpegSubtitlesJob::run ()
{
	dcpomatic_sleep (15);

	int64_t const len = _file_group.length ();
	while (true) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			break;
		}

		if (len > 0) {
			set_progress (float(_format_context->pb->pos) / len);
		} else {
			set_progress_unknown ();
		}

		if (_content->subtitle_stream() && _content->subtitle_stream()->uses_index(_format_context, _packet.stream_index) && _content->only_text()->use()) {
			int got_subtitle;
			AVSubtitle sub;
			if (avcodec_decode_subtitle2(subtitle_codec_context(), &sub, &got_subtitle, &_packet) >= 0 && got_subtitle) {
				for (unsigned int i = 0; i < sub.num_rects; ++i) {
					AVSubtitleRect const * rect = sub.rects[i];
					if (rect->type == SUBTITLE_BITMAP) {
#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
						/* sub_p looks up into a BGRA palette which is here
						   (i.e. first byte B, second G, third R, fourth A)
						*/
						uint32_t const * palette = (uint32_t *) rect->pict.data[1];
#else
						/* sub_p looks up into a BGRA palette which is here
						   (i.e. first byte B, second G, third R, fourth A)
						*/
						uint32_t const * palette = (uint32_t *) rect->data[1];
#endif
						RGBA c ((palette[i] & 0xff0000) >> 16, (palette[i] & 0xff00) >> 8, palette[i] & 0xff, (palette[i] & 0xff000000) >> 24);
						_content->subtitle_stream()->set_colour (c, c);
					}
				}
			}
		}

		av_packet_unref (&_packet);
	}

	set_progress (1);
	set_state (FINISHED_OK);
}
