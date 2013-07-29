/* caca.c -- a screen saving plugin for vlock,
 *           the VT locking program for linux
 *
 *  This file consists mostly of the code from cacademo from libcaca.  Only
 *  minor changes were necessary to fit it into vlock's module architecture.
 *  These changes are copyright (C) 2007 Frank Benkstein.
 *
 *  cacademo      various demo effects for libcaca
 *  Copyright (c) 1998 Michele Bini <mibin@tin.it>
 *                2003-2006 Jean-Yves Lamoureux <jylam@lnxscene.org>
 *                2004-2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  $Id: cacademo.c 1130 2007-06-28 12:49:28Z sam $
 *
 *  This program is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What The Fuck You Want
 *  To Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <ncurses.h>

#include <cucul.h>
#include <caca.h>

#include "process.h"

#include "vlock_plugin.h"

enum action { PREPARE, INIT, UPDATE, RENDER, FREE };

void transition(cucul_canvas_t *, int, int);
void plasma(enum action, cucul_canvas_t *);
void metaballs(enum action, cucul_canvas_t *);
void moire(enum action, cucul_canvas_t *);
void matrix(enum action, cucul_canvas_t *);

void (*fn[])(enum action, cucul_canvas_t *) =
{
    plasma,
    metaballs,
    moire,
    matrix,
};
#define DEMOS (sizeof(fn)/sizeof(*fn))

#define DEMO_FRAMES cucul_rand(500, 1000)
#define TRANSITION_FRAMES 40

#define TRANSITION_COUNT  3
#define TRANSITION_CIRCLE 0
#define TRANSITION_STAR   1
#define TRANSITION_SQUARE 2

/* Common macros for dither-based demos */
#define XSIZ 256
#define YSIZ 256

/* Global variables */
static int frame = 0;
static bool abort_requested = false;

void handle_sigterm(int __attribute__((unused)) signum)
{
  abort_requested = true;
}

static int caca_main(void *argument);

bool vlock_save(void **ctx_ptr)
{
  static struct child_process child = {
    .function = caca_main,
    .argument = NULL,
    .stdin_fd = REDIRECT_DEV_NULL,
    .stdout_fd = NO_REDIRECT,
    .stderr_fd = NO_REDIRECT,
  };

  /* Initialize ncurses. */
  initscr();

  if (!create_child(&child))
    return false;

  *ctx_ptr = &child;

  return true;
}

bool vlock_save_abort(void **ctx_ptr)
{
  struct child_process *child = *ctx_ptr;

  if (child != NULL) {
    ensure_death(child->pid);
    /* Restore sane terminal and uninitialize ncurses. */
    curs_set(1);
    refresh();
    endwin();
    *ctx_ptr = NULL;
  }

  return true;
}

static int caca_main(void __attribute__((unused)) *argument)
{
    static caca_display_t *dp;
    static cucul_canvas_t *frontcv, *backcv, *mask;

    int demo, next = -1, next_transition = DEMO_FRAMES;
    unsigned int i;
    int tmode = cucul_rand(0, TRANSITION_COUNT);

    /* Set up two canvases, a mask, and attach a display to the front one */
    frontcv = cucul_create_canvas(0, 0);
    backcv = cucul_create_canvas(0, 0);
    mask = cucul_create_canvas(0, 0);

    (void) setenv("CACA_DRIVER", "ncurses", 1);
    dp = caca_create_display(frontcv);

    if(!dp)
        return 1;

    cucul_set_canvas_size(backcv, cucul_get_canvas_width(frontcv),
                                  cucul_get_canvas_height(frontcv));
    cucul_set_canvas_size(mask, cucul_get_canvas_width(frontcv),
                                cucul_get_canvas_height(frontcv));

    /* Set refresh delay.  40ms corresponds to 25 FPS. */
    caca_set_display_time(dp, 40000);

    /* Initialise all demos' lookup tables */
    for(i = 0; i < DEMOS; i++)
        fn[i](PREPARE, frontcv);

    /* Choose a demo at random */
    demo = cucul_rand(0, DEMOS);
    fn[demo](INIT, frontcv);

    for(;;)
    {
        if (abort_requested)
          goto end;

        /* Resize the spare canvas, just in case the main one changed */
        cucul_set_canvas_size(backcv, cucul_get_canvas_width(frontcv),
                                      cucul_get_canvas_height(frontcv));
        cucul_set_canvas_size(mask, cucul_get_canvas_width(frontcv),
                                    cucul_get_canvas_height(frontcv));

        /* Update demo's data */
        fn[demo](UPDATE, frontcv);

        /* Handle transitions */
        if(frame == next_transition)
        {
            next = cucul_rand(0, DEMOS);
            if(next == demo)
                next = (next + 1) % DEMOS;
            fn[next](INIT, backcv);
        }
        else if(frame == next_transition + TRANSITION_FRAMES)
        {
            fn[demo](FREE, frontcv);
            demo = next;
            next = -1;
            next_transition = frame + DEMO_FRAMES;
            tmode = cucul_rand(0, TRANSITION_COUNT);
        }

        if(next != -1)
            fn[next](UPDATE, backcv);

        frame++;

        /* Render main demo's canvas */
        fn[demo](RENDER, frontcv);

        /* If a transition is on its way, render it */
        if(next != -1)
        {
            fn[next](RENDER, backcv);
            cucul_set_color_ansi(mask, CUCUL_LIGHTGRAY, CUCUL_BLACK);
            cucul_clear_canvas(mask);
            cucul_set_color_ansi(mask, CUCUL_WHITE, CUCUL_WHITE);
            transition(mask, tmode,
                       100 * (frame - next_transition) / TRANSITION_FRAMES);
            cucul_blit(frontcv, 0, 0, backcv, mask);
        }

        cucul_set_color_ansi(frontcv, CUCUL_WHITE, CUCUL_BLUE);
        if(frame < 100)
            cucul_put_str(frontcv, cucul_get_canvas_width(frontcv) - 30,
                                   cucul_get_canvas_height(frontcv) - 2,
                                   " -=[ Powered by libcaca ]=- ");
        caca_refresh_display(dp);
    }
end:
    if(next != -1)
        fn[next](FREE, frontcv);
    fn[demo](FREE, frontcv);

    caca_free_display(dp);
    cucul_free_canvas(mask);
    cucul_free_canvas(backcv);
    cucul_free_canvas(frontcv);

    return 0;
}

/* Transitions */
void transition(cucul_canvas_t *mask, int tmode, int completed)
{
    static float const star[] =
    {
         0.000000, -1.000000,
         0.308000, -0.349000,
         0.992000, -0.244000,
         0.500000,  0.266000,
         0.632000,  0.998000,
         0.008000,  0.659000,
        -0.601000,  0.995000,
        -0.496000,  0.275000,
        -0.997000, -0.244000,
        -0.313000, -0.349000
    };
    static float star_rot[sizeof(star)/sizeof(*star)];


    static float const square[] =
    {
        -1, -1,
        1, -1,
        1, 1,
        -1, 1
    };
    static float square_rot[sizeof(square)/sizeof(*square)];

    float mulx = 0.0075f * completed * cucul_get_canvas_width(mask);
    float muly = 0.0075f * completed * cucul_get_canvas_height(mask);
    int w2 = cucul_get_canvas_width(mask) / 2;
    int h2 = cucul_get_canvas_height(mask) / 2;
    float angle = (0.0075f * completed * 360) * 3.14 / 180, x, y;
    unsigned int i;

    switch(tmode)
    {
        case TRANSITION_SQUARE:
            /* Compute rotated coordinates */
            for(i = 0; i < (sizeof(square) / sizeof(*square)) / 2; i++)
            {
                x = square[i * 2];
                y = square[i * 2 + 1];

                square_rot[i * 2] = x * cos(angle) - y * sin(angle);
                square_rot[i * 2 + 1] = y * cos(angle) + x * sin(angle);
            }

            mulx *= 1.8;
            muly *= 1.8;
            cucul_fill_triangle(mask,
                                square_rot[0*2] * mulx + w2, square_rot[0*2+1] * muly + h2, \
                                square_rot[1*2] * mulx + w2, square_rot[1*2+1] * muly + h2, \
                                square_rot[2*2] * mulx + w2, square_rot[2*2+1] * muly + h2, '#');
            cucul_fill_triangle(mask,
                                square_rot[0*2] * mulx + w2, square_rot[0*2+1] * muly + h2, \
                                square_rot[2*2] * mulx + w2, square_rot[2*2+1] * muly + h2, \
                                square_rot[3*2] * mulx + w2, square_rot[3*2+1] * muly + h2, '#');
            break;


        case TRANSITION_STAR:
            /* Compute rotated coordinates */
            for(i = 0; i < (sizeof(star) / sizeof(*star)) / 2; i++)
            {
                x = star[i * 2];
                y = star[i * 2 + 1];

                star_rot[i * 2] = x * cos(angle) - y * sin(angle);
                star_rot[i * 2 + 1] = y * cos(angle) + x * sin(angle);
            }

            mulx *= 1.8;
            muly *= 1.8;

#define DO_TRI(a, b, c) \
    cucul_fill_triangle(mask, \
        star_rot[(a)*2] * mulx + w2, star_rot[(a)*2+1] * muly + h2, \
        star_rot[(b)*2] * mulx + w2, star_rot[(b)*2+1] * muly + h2, \
        star_rot[(c)*2] * mulx + w2, star_rot[(c)*2+1] * muly + h2, '#')
            DO_TRI(0, 1, 9);
            DO_TRI(1, 2, 3);
            DO_TRI(3, 4, 5);
            DO_TRI(5, 6, 7);
            DO_TRI(7, 8, 9);
            DO_TRI(9, 1, 5);
            DO_TRI(9, 5, 7);
            DO_TRI(1, 3, 5);
            break;

        case TRANSITION_CIRCLE:
            cucul_fill_ellipse(mask, w2, h2, mulx, muly, '#');
            break;

    }
}

/* The plasma effect */
#define TABLEX (XSIZ * 2)
#define TABLEY (YSIZ * 2)
static uint8_t table[TABLEX * TABLEY];

static void do_plasma(uint8_t *,
                      double, double, double, double, double, double);

void plasma(enum action action, cucul_canvas_t *cv)
{
    static cucul_dither_t *dither;
    static uint8_t *screen;
    static unsigned int red[256], green[256], blue[256], alpha[256];
    static double r[3], R[6];

    int i, x, y;

    switch(action)
    {
    case PREPARE:
        /* Fill various tables */
        for(i = 0 ; i < 256; i++)
            red[i] = green[i] = blue[i] = alpha[i] = 0;

        for(i = 0; i < 3; i++)
            r[i] = (double)(cucul_rand(1, 1000)) / 60000 * M_PI;

        for(i = 0; i < 6; i++)
            R[i] = (double)(cucul_rand(1, 1000)) / 10000;

        for(y = 0 ; y < TABLEY ; y++)
            for(x = 0 ; x < TABLEX ; x++)
        {
            double tmp = (((double)((x - (TABLEX / 2)) * (x - (TABLEX / 2))
                                  + (y - (TABLEX / 2)) * (y - (TABLEX / 2))))
                          * (M_PI / (TABLEX * TABLEX + TABLEY * TABLEY)));

            table[x + y * TABLEX] = (1.0 + sin(12.0 * sqrt(tmp))) * 256 / 6;
        }
        break;

    case INIT:
        screen = malloc(XSIZ * YSIZ * sizeof(uint8_t));
        dither = cucul_create_dither(8, XSIZ, YSIZ, XSIZ, 0, 0, 0, 0);
        break;

    case UPDATE:
        for(i = 0 ; i < 256; i++)
        {
            double z = ((double)i) / 256 * 6 * M_PI;

            red[i] = (1.0 + sin(z + r[1] * frame)) / 2 * 0xfff;
            blue[i] = (1.0 + cos(z + r[0] * (frame + 100))) / 2 * 0xfff;
            green[i] = (1.0 + cos(z + r[2] * (frame + 200))) / 2 * 0xfff;
        }

        /* Set the palette */
        cucul_set_dither_palette(dither, red, green, blue, alpha);

        do_plasma(screen,
                  (1.0 + sin(((double)frame) * R[0])) / 2,
                  (1.0 + sin(((double)frame) * R[1])) / 2,
                  (1.0 + sin(((double)frame) * R[2])) / 2,
                  (1.0 + sin(((double)frame) * R[3])) / 2,
                  (1.0 + sin(((double)frame) * R[4])) / 2,
                  (1.0 + sin(((double)frame) * R[5])) / 2);
        break;

    case RENDER:
        cucul_dither_bitmap(cv, 0, 0,
                            cucul_get_canvas_width(cv),
                            cucul_get_canvas_height(cv),
                            dither, screen);
        break;

    case FREE:
        free(screen);
        cucul_free_dither(dither);
        break;
    }
}

static void do_plasma(uint8_t *pixels, double x_1, double y_1,
                      double x_2, double y_2, double x_3, double y_3)
{
    unsigned int X1 = x_1 * (TABLEX / 2),
                 Y1 = y_1 * (TABLEY / 2),
                 X2 = x_2 * (TABLEX / 2),
                 Y2 = y_2 * (TABLEY / 2),
                 X3 = x_3 * (TABLEX / 2),
                 Y3 = y_3 * (TABLEY / 2);
    unsigned int y;
    uint8_t * t1 = table + X1 + Y1 * TABLEX,
            * t2 = table + X2 + Y2 * TABLEX,
            * t3 = table + X3 + Y3 * TABLEX;

    for(y = 0; y < YSIZ; y++)
    {
        unsigned int x;
        uint8_t * tmp = pixels + y * YSIZ;
        unsigned int ty = y * TABLEX, tmax = ty + XSIZ;
        for(x = 0; ty < tmax; ty++, tmp++)
            tmp[0] = t1[ty] + t2[ty] + t3[ty];
    }
}

/* The metaball effect */
#define METASIZE (XSIZ/2)
#define METABALLS 12
#define CROPBALL 200 /* Colour index where to crop balls */
static uint8_t metaball[METASIZE * METASIZE];

static void create_ball(void);
static void draw_ball(uint8_t *, unsigned int, unsigned int);

void metaballs(enum action action, cucul_canvas_t *cv)
{
    static cucul_dither_t *cucul_dither;
    static uint8_t *screen;
    static unsigned int r[256], g[256], b[256], a[256];
    static float dd[METABALLS], di[METABALLS], dj[METABALLS], dk[METABALLS];
    static unsigned int x[METABALLS], y[METABALLS];
    static float i = 10.0, j = 17.0, k = 11.0;
    static double offset[360 + 80];
    static unsigned int angleoff;

    int n, angle;

    switch(action)
    {
    case PREPARE:
        /* Make the palette eatable by libcaca */
        for(n = 0; n < 256; n++)
            r[n] = g[n] = b[n] = a[n] = 0x0;
        r[255] = g[255] = b[255] = 0xfff;

        /* Generate ball sprite */
        create_ball();

        for(n = 0; n < METABALLS; n++)
        {
            dd[n] = cucul_rand(0, 100);
            di[n] = (float)cucul_rand(500, 4000) / 6000.0;
            dj[n] = (float)cucul_rand(500, 4000) / 6000.0;
            dk[n] = (float)cucul_rand(500, 4000) / 6000.0;
        }

        angleoff = cucul_rand(0, 360);

        for(n = 0; n < 360 + 80; n++)
            offset[n] = 1.0 + sin((double)(n * M_PI / 60));
        break;

    case INIT:
        screen = malloc(XSIZ * YSIZ * sizeof(uint8_t));
        /* Create a libcucul dither smaller than our pixel buffer, so that we
         * display only the interesting part of it */
        cucul_dither = cucul_create_dither(8, XSIZ - METASIZE, YSIZ - METASIZE,
                                           XSIZ, 0, 0, 0, 0);
        break;

    case UPDATE:
        angle = (frame + angleoff) % 360;

        /* Crop the palette */
        for(n = CROPBALL; n < 255; n++)
        {
            int t1, t2, t3;
            double c1 = offset[angle];
            double c2 = offset[angle + 40];
            double c3 = offset[angle + 80];

            t1 = n < 0x40 ? 0 : n < 0xc0 ? (n - 0x40) * 0x20 : 0xfff;
            t2 = n < 0xe0 ? 0 : (n - 0xe0) * 0x80;
            t3 = n < 0x40 ? n * 0x40 : 0xfff;

            r[n] = (c1 * t1 + c2 * t2 + c3 * t3) / 4;
            g[n] = (c1 * t2 + c2 * t3 + c3 * t1) / 4;
            b[n] = (c1 * t3 + c2 * t1 + c3 * t2) / 4;
        }

        /* Set the palette */
        cucul_set_dither_palette(cucul_dither, r, g, b, a);

        /* Silly paths for our balls */
        for(n = 0; n < METABALLS; n++)
        {
            float u = di[n] * i + dj[n] * j + dk[n] * sin(di[n] * k);
            float v = dd[n] + di[n] * j + dj[n] * k + dk[n] * sin(dk[n] * i);
            u = sin(i + u * 2.1) * (1.0 + sin(u));
            v = sin(j + v * 1.9) * (1.0 + sin(v));
            x[n] = (XSIZ - METASIZE) / 2 + u * (XSIZ - METASIZE) / 4;
            y[n] = (YSIZ - METASIZE) / 2 + v * (YSIZ - METASIZE) / 4;
        }

        i += 0.011;
        j += 0.017;
        k += 0.019;

        memset(screen, 0, XSIZ * YSIZ);

        for(n = 0; n < METABALLS; n++)
            draw_ball(screen, x[n], y[n]);
        break;

    case RENDER:
        cucul_dither_bitmap(cv, 0, 0,
                          cucul_get_canvas_width(cv),
                          cucul_get_canvas_height(cv),
                          cucul_dither, screen + (METASIZE / 2) * (1 + XSIZ));
        break;

    case FREE:
        free(screen);
        cucul_free_dither(cucul_dither);
        break;
    }
}

static void create_ball(void)
{
    int x, y;
    float distance;

    for(y = 0; y < METASIZE; y++)
        for(x = 0; x < METASIZE; x++)
    {
        distance = ((METASIZE/2) - x) * ((METASIZE/2) - x)
                 + ((METASIZE/2) - y) * ((METASIZE/2) - y);
        distance = sqrt(distance) * 64 / METASIZE;
        metaball[x + y * METASIZE] = distance > 15 ? 0 : (255 - distance) * 15;
    }
}

static void draw_ball(uint8_t *screen, unsigned int bx, unsigned int by)
{
    unsigned int color;
    unsigned int i, e = 0;
    unsigned int b = (by * XSIZ) + bx;

    for(i = 0; i < METASIZE * METASIZE; i++)
    {
        color = screen[b] + metaball[i];

        if(color > 255)
            color = 255;

        screen[b] = color;
        if(e == METASIZE)
        {
            e = 0;
            b += XSIZ - METASIZE;
        }
        b++;
        e++;
    }
}

/* The moir� effect */
#define DISCSIZ (XSIZ*2)
#define DISCTHICKNESS (XSIZ*15/40)
static uint8_t disc[DISCSIZ * DISCSIZ];

static void put_disc(uint8_t *, int, int);
static void draw_line(int, int, char);

void moire(enum action action, cucul_canvas_t *cv)
{
    static cucul_dither_t *dither;
    static uint8_t *screen;
    static float d[6];
    static unsigned int red[256], green[256], blue[256], alpha[256];

    int i, x, y;

    switch(action)
    {
    case PREPARE:
        /* Fill various tables */
        for(i = 0 ; i < 256; i++)
            red[i] = green[i] = blue[i] = alpha[i] = 0;

        for(i = 0; i < 6; i++)
            d[i] = ((float)cucul_rand(50, 70)) / 1000.0;

        red[0] = green[0] = blue[0] = 0x777;
        red[1] = green[1] = blue[1] = 0xfff;

        /* Fill the circle */
        for(i = DISCSIZ * 2; i > 0; i -= DISCTHICKNESS)
        {
            int t, dx, dy;

            for(t = 0, dx = 0, dy = i; dx <= dy; dx++)
            {
                draw_line(dx / 3, dy / 3, (i / DISCTHICKNESS) % 2);
                draw_line(dy / 3, dx / 3, (i / DISCTHICKNESS) % 2);

                t += t > 0 ? dx - dy-- : dx;
            }
        }

        break;

    case INIT:
        screen = malloc(XSIZ * YSIZ * sizeof(uint8_t));
        dither = cucul_create_dither(8, XSIZ, YSIZ, XSIZ, 0, 0, 0, 0);
        break;

    case UPDATE:
        memset(screen, 0, XSIZ * YSIZ);

        /* Set the palette */
        red[0] = 0.5 * (1 + sin(d[0] * (frame + 1000))) * 0xfff;
        green[0] = 0.5 * (1 + cos(d[1] * frame)) * 0xfff;
        blue[0] = 0.5 * (1 + cos(d[2] * (frame + 3000))) * 0xfff;

        red[1] = 0.5 * (1 + sin(d[3] * (frame + 2000))) * 0xfff;
        green[1] = 0.5 * (1 + cos(d[4] * frame + 5.0)) * 0xfff;
        blue[1] = 0.5 * (1 + cos(d[5] * (frame + 4000))) * 0xfff;

        cucul_set_dither_palette(dither, red, green, blue, alpha);

        /* Draw circles */
        x = cos(d[0] * (frame + 1000)) * 128.0 + (XSIZ / 2);
        y = sin(0.11 * frame) * 128.0 + (YSIZ / 2);
        put_disc(screen, x, y);

        x = cos(0.13 * frame + 2.0) * 64.0 + (XSIZ / 2);
        y = sin(d[1] * (frame + 2000)) * 64.0 + (YSIZ / 2);
        put_disc(screen, x, y);
        break;

    case RENDER:
        cucul_dither_bitmap(cv, 0, 0,
                            cucul_get_canvas_width(cv),
                            cucul_get_canvas_height(cv),
                            dither, screen);
        break;

    case FREE:
        free(screen);
        cucul_free_dither(dither);
        break;
    }
}

static void put_disc(uint8_t *screen, int x, int y)
{
    char *src = ((char*)disc) + (DISCSIZ / 2 - x) + (DISCSIZ / 2 - y) * DISCSIZ;
    int i, j;

    for(j = 0; j < YSIZ; j++)
        for(i = 0; i < XSIZ; i++)
    {
        screen[i + XSIZ * j] ^= src[i + DISCSIZ * j];
    }
}

static void draw_line(int x, int y, char color)
{
    if(x == 0 || y == 0 || y > DISCSIZ / 2)
        return;

    if(x > DISCSIZ / 2)
        x = DISCSIZ / 2;

    memset(disc + (DISCSIZ / 2) - x + DISCSIZ * ((DISCSIZ / 2) - y),
           color, 2 * x - 1);
    memset(disc + (DISCSIZ / 2) - x + DISCSIZ * ((DISCSIZ / 2) + y - 1),
           color, 2 * x - 1);
}

/* Matrix effect */
#define MAXDROPS 500
#define MINLEN 15
#define MAXLEN 30

void matrix(enum action action, cucul_canvas_t *cv)
{
    static struct drop
    {
        int x, y, speed, len;
        char str[MAXLEN];
    }
    drop[MAXDROPS];

    int w, h, i, j;

    switch(action)
    {
    case PREPARE:
        for(i = 0; i < MAXDROPS; i++)
        {
            drop[i].x = cucul_rand(0, 1000);
            drop[i].y = cucul_rand(0, 1000);
            drop[i].speed = 5 + cucul_rand(0, 30);
            drop[i].len = MINLEN + cucul_rand(0, (MAXLEN - MINLEN));
            for(j = 0; j < MAXLEN; j++)
                drop[i].str[j] = cucul_rand('0', 'z');
        }
        break;

    case INIT:
        break;

    case UPDATE:
        w = cucul_get_canvas_width(cv);
        h = cucul_get_canvas_height(cv);

        for(i = 0; i < MAXDROPS && i < (w * h / 32); i++)
        {
            drop[i].y += drop[i].speed;
            if(drop[i].y > 1000)
            {
                drop[i].y -= 1000;
                drop[i].x = cucul_rand(0, 1000);
            }
        }
        break;

    case RENDER:
        w = cucul_get_canvas_width(cv);
        h = cucul_get_canvas_height(cv);

        cucul_set_color_ansi(cv, CUCUL_BLACK, CUCUL_BLACK);
        cucul_clear_canvas(cv);

        for(i = 0; i < MAXDROPS && i < (w * h / 32); i++)
        {
            int x, y;

            x = drop[i].x * w / 1000 / 2 * 2;
            y = drop[i].y * (h + MAXLEN) / 1000;

            for(j = 0; j < drop[i].len; j++)
            {
                unsigned int fg;

                if(j < 2)
                    fg = CUCUL_WHITE;
                else if(j < drop[i].len / 4)
                    fg = CUCUL_LIGHTGREEN;
                else if(j < drop[i].len * 4 / 5)
                    fg = CUCUL_GREEN;
                else
                    fg = CUCUL_DARKGRAY;
                cucul_set_color_ansi(cv, fg, CUCUL_BLACK);

                cucul_put_char(cv, x, y - j,
                               drop[i].str[(y - j) % drop[i].len]);
            }
        }
        break;

    case FREE:
        break;
    }
}
