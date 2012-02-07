gcc -D__STDC_CONSTANT_MACROS -c -Wall -std=c99 -o test.o test.c && gcc -o test test.o `pkg-config --cflags --libs libavformat libswscale`
