#include <libavutil/pixfmt.h>
#include <stdio.h>

#define SHOW(x) printf("%d " #x "\n", x);
int main()
{
	SHOW(AV_PIX_FMT_YUV420P);
	SHOW(AV_PIX_FMT_ARGB);
	SHOW(AV_PIX_FMT_RGBA);
	SHOW(AV_PIX_FMT_ABGR);
	SHOW(AV_PIX_FMT_BGRA);
	SHOW(AV_PIX_FMT_YUV420P16LE);
	SHOW(AV_PIX_FMT_YUV422P10LE);
	SHOW(AV_PIX_FMT_YUV444P9BE);
}
