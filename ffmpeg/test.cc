#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

using namespace std;

char const video[] = "./starwars.mpg";//Ghostbusters.avi";

static void
save_frame (AVFrame *frame, int width, int height, int N)
{
	char filename[32];
	
	sprintf (filename, "frame%d.ppm", N);
	FILE* file = fopen (filename, "wb");
	if (file == NULL) {
		return;
	}

	fprintf (file, "P6\n%d %d\n255\n", width, height);

	for (int y = 0; y < height; y++) {
		fwrite (frame->data[0] + y * frame->linesize[0], 1, width * 3, file);
	}

	fclose (file);
}

int
main (int argc, char* argv[])
{
	av_register_all ();

	AVFormatContext* format_context = NULL;
	if (avformat_open_input (&format_context, video, NULL, NULL) != 0) {
		fprintf (stderr, "avformat_open_input failed.\n");
		return -1;
	}

	if (avformat_find_stream_info (format_context, NULL) < 0) {
		fprintf (stderr, "av_find_stream_info failed.\n");
		return -1;
	}

	av_dump_format (format_context, 0, video, 0);

	int video_stream = -1;
	for (uint32_t i = 0; i < format_context->nb_streams; ++i) {
		if (format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream = i;
			break;
		}
	}

	AVCodecContext* decoder_context = format_context->streams[video_stream]->codec;

	AVCodec* decoder = avcodec_find_decoder (decoder_context->codec_id);
	if (decoder == NULL) {
		fprintf (stderr, "avcodec_find_decoder failed.\n");
		return -1;
	}

	if (avcodec_open2 (decoder_context, decoder, NULL) < 0) {
		fprintf (stderr, "avcodec_open failed.\n");
		return -1;
	}

	/* XXX */
	if (decoder_context->time_base.num > 1000 && decoder_context->time_base.den == 1) {
		decoder_context->time_base.den = 1000;
	}

	AVCodec* encoder = avcodec_find_encoder (CODEC_ID_JPEG2000);
	if (encoder == NULL) {
		cerr << "avcodec_find_encoder failed.\n";
		return -1;
	}

	AVCodecContext* encoder_context = avcodec_alloc_context3 (encoder);

	cout << decoder_context->width << " x " << decoder_context->height << "\n";
	encoder_context->width = decoder_context->width;
	encoder_context->height = decoder_context->height;
	/* XXX */
	encoder_context->time_base = (AVRational) {1, 25};
	encoder_context->pix_fmt = PIX_FMT_YUV420P;
	encoder_context->compression_level = 0;

	if (avcodec_open2 (encoder_context, encoder, NULL) < 0) {
		cerr << "avcodec_open failed.\n";
		return -1;
	}
	
	AVFrame* frame = avcodec_alloc_frame ();

	AVFrame* frame_RGB = avcodec_alloc_frame ();
	if (frame_RGB == NULL) {
		fprintf (stderr, "avcodec_alloc_frame failed.\n");
		return -1;
	}

	FILE* outfile = fopen ("output", "wb");

	int num_bytes = avpicture_get_size (PIX_FMT_RGB24, decoder_context->width, decoder_context->height);
	uint8_t* buffer = (uint8_t *) malloc (num_bytes);

	avpicture_fill ((AVPicture *) frame_RGB, buffer, PIX_FMT_RGB24, decoder_context->width, decoder_context->height);

	/* alloc image and output buffer */
	int outbuf_size = 100000;
	uint8_t* outbuf = (uint8_t *) malloc (outbuf_size);
	int out_size = 0;
	
	AVPacket packet;
	int i = 0;
	while (av_read_frame (format_context, &packet) >= 0) {

		int frame_finished;

		if (packet.stream_index == video_stream) {
			avcodec_decode_video2 (decoder_context, frame, &frame_finished, &packet);

			if (frame_finished) {
				static struct SwsContext *img_convert_context;

				if (img_convert_context == NULL) {
					int w = decoder_context->width;
					int h = decoder_context->height;
					
					img_convert_context = sws_getContext (
						w, h, 
						decoder_context->pix_fmt, 
						w, h, PIX_FMT_RGB24, SWS_BICUBIC,
						NULL, NULL, NULL
						);
					
					if (img_convert_context == NULL) {
						fprintf (stderr, "sws_getContext failed.\n");
						return -1;
					}
				}
				
				sws_scale (
					img_convert_context, frame->data, frame->linesize, 0, 
					decoder_context->height, frame_RGB->data, frame_RGB->linesize
					);

				out_size = avcodec_encode_video (encoder_context, outbuf, outbuf_size, frame);
				++i;
				if (i == 200) {
					fwrite (outbuf, 1, out_size, outfile);
				}
			}
		}

		av_free_packet (&packet);
	}

	while (out_size) {
		out_size = avcodec_encode_video (encoder_context, outbuf, outbuf_size, NULL);
		fwrite (outbuf, 1, out_size, outfile);
	}

	fclose (outfile);
	free (buffer);
	av_free (frame_RGB);
	av_free (frame);
	avcodec_close (decoder_context);
	avcodec_close (encoder_context);
	avformat_close_input (&format_context);
	
	return 0;	
}
