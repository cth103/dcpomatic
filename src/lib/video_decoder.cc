#include "video_decoder.h"
#include "subtitle.h"
#include "film.h"
#include "image.h"
#include "log.h"
#include "options.h"
#include "job.h"

using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<Film> f, shared_ptr<const Options> o, Job* j)
	: Decoder (f, o, j)
	, _video_frame (0)
{

}

/** Called by subclasses to tell the world that some video data is ready.
 *  We find a subtitle then emit it for listeners.
 *  @param frame to decode; caller manages memory.
 */
void
VideoDecoder::emit_video (shared_ptr<Image> image)
{
	shared_ptr<Subtitle> sub;
	if (_timed_subtitle && _timed_subtitle->displayed_at (double (video_frame()) / _film->frames_per_second())) {
		sub = _timed_subtitle->subtitle ();
	}

	signal_video (image, sub);
}

void
VideoDecoder::repeat_last_video ()
{
	if (!_last_image) {
		_last_image.reset (new CompactImage (pixel_format(), native_size()));
		_last_image->make_black ();
	}

	signal_video (_last_image, _last_subtitle);
}

void
VideoDecoder::signal_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	TIMING ("Decoder emits %1", _video_frame);
	Video (image, sub);
	++_video_frame;

	_last_image = image;
	_last_subtitle = sub;
}

void
VideoDecoder::emit_subtitle (shared_ptr<TimedSubtitle> s)
{
	_timed_subtitle = s;
	
	if (_timed_subtitle && _opt->apply_crop) {
		Position const p = _timed_subtitle->subtitle()->position ();
		_timed_subtitle->subtitle()->set_position (Position (p.x - _film->crop().left, p.y - _film->crop().top));
	}
}

void
VideoDecoder::set_subtitle_stream (optional<SubtitleStream> s)
{
	_subtitle_stream = s;
}

void
VideoDecoder::set_progress () const
{
	if (_job && _film->dcp_length()) {
		_job->set_progress (float (_video_frame) / _film->length().get());
	}
}
