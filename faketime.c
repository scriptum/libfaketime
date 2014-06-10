/*******************************************************************************

Copyright (c) 2014 Pavel Roschin <rpg89@post.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions: The above
copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*******************************************************************************/

#include "libpreload.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

enum {OFF_GETTIMEOFDAY = 0, OFF_MONOTONIC, OFF_MONOTONIC_RAW, OFF_COUNT};

long double globaltime[OFF_COUNT];
long double offsettime[OFF_COUNT];

Display *display;
pthread_t th;
int end = 0;
long double speed = 1.0L;

static int
check_key(KeySym keysym, char keys_return[32])
{
	KeyCode code = XKeysymToKeycode(display, keysym);
	return keys_return[code / 8] & (1 << (code % 8));
}

static void *
x11events(void *data __attribute__((unused)))
{
	while(!end)
	{
		char keys_return[32];
		speed = 1.0L;
		XQueryKeymap(display, keys_return);
		if(check_key(XK_Shift_R, keys_return))
			speed *= 3.0L;
		if(check_key(XK_Shift_L, keys_return))
			speed *= 1.5L;
		if(check_key(XK_Control_R, keys_return))
			speed /= 3.0L;
		if(check_key(XK_Control_L, keys_return))
			speed /= 1.5L;
		usleep(20000);
	}
	return NULL;
}

__attribute__((constructor))
static void
initlib()
{
	unsetenv("LD_PRELOAD");
	XInitThreads();
	display = XOpenDisplay((char*)0);
	pthread_create(&th, 0, x11events, 0);
}

__attribute__((destructor))
static void
closelib()
{
	end = 1;
	pthread_join(th, NULL);
}

static inline long double
tv2ld(struct timeval *tv)
{
	return (long double)tv->tv_sec + (long double)tv->tv_usec / 1000000.L;
}

static inline long double
tp2ld(struct timespec *tp)
{
	return (long double)tp->tv_sec + (long double)tp->tv_nsec / 1000000000.L;
}

static long double
calc_speed(int off, long double delta)
{
	delta = globaltime[off] - delta;
	offsettime[off] += (1.0L - speed) * delta;
	return globaltime[off] - offsettime[off];
}

static long double
gettimeofdayld(struct timezone *tz, int *ret);

HIDE_START(int, gettimeofday, (struct timeval *tv, struct timezone *tz))
{
	int ret;
	if(first_run)
	{
		globaltime[OFF_GETTIMEOFDAY] = gettimeofdayld(tz, &ret);
		offsettime[OFF_GETTIMEOFDAY] = 0.L;
	}
	long double delta = globaltime[0];
	globaltime[0] = gettimeofdayld(tz, &ret);
	if (ret == 0 && tv)
	{
		delta = calc_speed(0, delta);
		// printf("%Lf %Lf\n", delta, globaltime[0]);
		tv->tv_sec = delta;
		tv->tv_usec = (delta - (long double)tv->tv_sec) * 1000000.L;
	}

	return ret;
}
HIDE_END

static long double
gettimeofdayld(struct timezone *tz, int *ret)
{
	struct timeval tv;

	*ret = gettimeofday_orig(&tv, tz);
	if(*ret == 0)
		return tv2ld(&tv);
	return 0.L;
}

static int
clk_id_to_off(clockid_t clk_id)
{
	switch(clk_id)
	{
		case CLOCK_MONOTONIC:
			return OFF_MONOTONIC;
		case CLOCK_MONOTONIC_RAW:
			return OFF_MONOTONIC_RAW;
	}
	return -1;
}

static long double
clock_gettimeld(clockid_t clk_id, int *ret);

HIDE_START(int, clock_gettime, (clockid_t clk_id, struct timespec *tp))
{
	int ret, off;

	off = clk_id_to_off(clk_id);

	if(first_run)
	{
		globaltime[OFF_MONOTONIC] = clock_gettimeld(CLOCK_MONOTONIC, &ret);
		globaltime[OFF_MONOTONIC_RAW] = clock_gettimeld(CLOCK_MONOTONIC_RAW, &ret);
		offsettime[OFF_MONOTONIC] = 0.L;
		offsettime[OFF_MONOTONIC_RAW] = 0.L;
	}

	if(off == -1)
	{
		return clock_gettime_orig(clk_id, tp);
	}
	long double delta = globaltime[off];
	globaltime[off] = clock_gettimeld(clk_id, &ret);
	if(ret == 0 && tp)
	{
		delta = calc_speed(off, delta);
		tp->tv_sec = delta;
		tp->tv_nsec = (delta - (long double)tp->tv_sec) * 1000000000.L;
	}

	return ret; 
}
HIDE_END

static long double
clock_gettimeld(clockid_t clk_id, int *ret)
{
	struct timespec tp;

	*ret = clock_gettime_orig(clk_id, &tp);
	if(*ret == 0)
	{
		return tp2ld(&tp);
	}
	return 0.L;
}
