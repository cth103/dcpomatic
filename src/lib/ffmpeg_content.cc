#include "ffmpeg_content.h"
#include "ffmpeg_decoder.h"
#include "options.h"
#include "compose.hpp"
#include "job.h"
#include "util.h"
#include "log.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

int const FFmpegContentProperty::SUBTITLE_STREAMS = 100;
int const FFmpegContentProperty::SUBTITLE_STREAM = 101;
int const FFmpegContentProperty::AUDIO_STREAMS = 102;
int const FFmpegContentProperty::AUDIO_STREAM = 103;

FFmpegContent::FFmpegContent (boost::filesystem::path f)
	: Content (f)
	, VideoContent (f)
	, AudioContent (f)
{

}

void
FFmpegContent::examine (shared_ptr<Film> film, shared_ptr<Job> job, bool quick)
{
	job->descend (0.5);
	Content::examine (film, job, quick);
	job->ascend ();

	job->set_progress_unknown ();

	DecodeOptions o;
	o.decode_audio = false;
	shared_ptr<FFmpegDecoder> decoder (new FFmpegDecoder (film, shared_from_this (), o));

	ContentVideoFrame video_length = 0;
	if (quick) {
		video_length = decoder->video_length ();
                film->log()->log (String::compose ("Video length obtained from header as %1 frames", decoder->video_length ()));
        } else {
                while (!decoder->pass ()) {
                        /* keep going */
                }

                video_length = decoder->video_frame ();
                film->log()->log (String::compose ("Video length examined as %1 frames", decoder->video_frame ()));
        }

        {
                boost::mutex::scoped_lock lm (_mutex);

                _video_length = video_length;

                _subtitle_streams = decoder->subtitle_streams ();
                if (!_subtitle_streams.empty ()) {
                        _subtitle_stream = _subtitle_streams.front ();
                }
                
                _audio_streams = decoder->audio_streams ();
                if (!_audio_streams.empty ()) {
                        _audio_stream = _audio_streams.front ();
                }
        }

        take_from_video_decoder (decoder);

        Changed (VideoContentProperty::VIDEO_LENGTH);
        Changed (FFmpegContentProperty::SUBTITLE_STREAMS);
        Changed (FFmpegContentProperty::SUBTITLE_STREAM);
        Changed (FFmpegContentProperty::AUDIO_STREAMS);
        Changed (FFmpegContentProperty::AUDIO_STREAM);
}

string
FFmpegContent::summary () const
{
	return String::compose (_("Movie: %1"), file().filename ());
}

void
FFmpegContent::set_subtitle_stream (FFmpegSubtitleStream s)
{
        {
                boost::mutex::scoped_lock lm (_mutex);
                _subtitle_stream = s;
        }

        Changed (FFmpegContentProperty::SUBTITLE_STREAM);
}

void
FFmpegContent::set_audio_stream (FFmpegAudioStream s)
{
        {
                boost::mutex::scoped_lock lm (_mutex);
                _audio_stream = s;
        }

        Changed (FFmpegContentProperty::AUDIO_STREAM);
}

ContentAudioFrame
FFmpegContent::audio_length () const
{
        if (!_audio_stream) {
                return 0;
        }
        
        return video_frames_to_audio_frames (_video_length, audio_frame_rate(), video_frame_rate());
}

int
FFmpegContent::audio_channels () const
{
        if (!_audio_stream) {
                return 0;
        }

        return _audio_stream->channels ();
}

int
FFmpegContent::audio_frame_rate () const
{
        if (!_audio_stream) {
                return 0;
        }

        return _audio_stream->frame_rate;
}

int64_t
FFmpegContent::audio_channel_layout () const
{
        if (!_audio_stream) {
                return 0;
        }

        return _audio_stream->channel_layout;
}
	
bool
operator== (FFmpegSubtitleStream const & a, FFmpegSubtitleStream const & b)
{
        return a.id == b.id;
}

bool
operator== (FFmpegAudioStream const & a, FFmpegAudioStream const & b)
{
        return a.id == b.id;
}
