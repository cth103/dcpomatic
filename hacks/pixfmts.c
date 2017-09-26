#include <libavutil/pixfmt.h>
#include <stdio.h>

#define SHOW(x) printf("%d " #x "\n", x);
int main()
{
	SHOW(AV_PIX_FMT_YUV420P);
	SHOW(AV_PIX_FMT_YUV420P16LE);
	SHOW(AV_PIX_FMT_YUV422P10LE);
	SHOW(AV_PIX_FMT_YUV444P9BE);
}
