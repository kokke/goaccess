/**
 * output.c -- output to the standard output stream
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public License is attached to this
 * source distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <locale.h>

#include "output.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "commons.h"
#include "error.h"
#include "gdns.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */

static void print_html_visitors (FILE * fp, GHolder * h, int processed, const GOutput * panel);
static void print_html_requests (FILE * fp, GHolder * h, int processed, const GOutput * panel);
static void print_html_common (FILE * fp, GHolder * h, int processed, const GOutput * panel);

static GOutput paneling[] = {
  {VISITORS        , print_html_visitors , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 0} ,
  {REQUESTS        , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {REQUESTS_STATIC , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {NOT_FOUND       , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {HOSTS           , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 0} ,
  {OS              , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 1} ,
  {BROWSERS        , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 1} ,
  {REFERRERS       , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
  {REFERRING_SITES , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
  {KEYPHRASES      , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
#endif
  {STATUS_CODES    , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
};

/* base64 expand/collapse icons */
unsigned char icons [] = {
  0x64, 0x30, 0x39, 0x47, 0x52, 0x67, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x56, 0x38, 0x41, 0x41, 0x73, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x42, 0x54, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x50,
  0x55, 0x79, 0x38, 0x79, 0x41, 0x41, 0x41, 0x42, 0x43, 0x41, 0x41, 0x41,
  0x41, 0x47, 0x41, 0x41, 0x41, 0x41, 0x42, 0x67, 0x44, 0x78, 0x49, 0x4e,
  0x6b, 0x47, 0x4e, 0x74, 0x59, 0x58, 0x41, 0x41, 0x41, 0x41, 0x46, 0x6f,
  0x41, 0x41, 0x41, 0x41, 0x54, 0x41, 0x41, 0x41, 0x41, 0x45, 0x77, 0x50,
  0x38, 0x4f, 0x45, 0x69, 0x5a, 0x32, 0x46, 0x7a, 0x63, 0x41, 0x41, 0x41,
  0x41, 0x62, 0x51, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x41, 0x41, 0x41, 0x41, 0x42, 0x42, 0x6e, 0x62, 0x48, 0x6c, 0x6d,
  0x41, 0x41, 0x41, 0x42, 0x76, 0x41, 0x41, 0x41, 0x41, 0x62, 0x51, 0x41,
  0x41, 0x41, 0x47, 0x30, 0x52, 0x52, 0x42, 0x73, 0x6c, 0x47, 0x68, 0x6c,
  0x59, 0x57, 0x51, 0x41, 0x41, 0x41, 0x4e, 0x77, 0x41, 0x41, 0x41, 0x41,
  0x4e, 0x67, 0x41, 0x41, 0x41, 0x44, 0x59, 0x45, 0x6c, 0x7a, 0x43, 0x5a,
  0x61, 0x47, 0x68, 0x6c, 0x59, 0x51, 0x41, 0x41, 0x41, 0x36, 0x67, 0x41,
  0x41, 0x41, 0x41, 0x6b, 0x41, 0x41, 0x41, 0x41, 0x4a, 0x41, 0x63, 0x77,
  0x41, 0x38, 0x64, 0x6f, 0x62, 0x58, 0x52, 0x34, 0x41, 0x41, 0x41, 0x44,
  0x7a, 0x41, 0x41, 0x41, 0x41, 0x42, 0x67, 0x41, 0x41, 0x41, 0x41, 0x59,
  0x43, 0x67, 0x41, 0x41, 0x42, 0x32, 0x78, 0x76, 0x59, 0x32, 0x45, 0x41,
  0x41, 0x41, 0x50, 0x6b, 0x41, 0x41, 0x41, 0x41, 0x44, 0x67, 0x41, 0x41,
  0x41, 0x41, 0x34, 0x42, 0x41, 0x67, 0x43, 0x51, 0x62, 0x57, 0x46, 0x34,
  0x63, 0x41, 0x41, 0x41, 0x41, 0x2f, 0x51, 0x41, 0x41, 0x41, 0x41, 0x67,
  0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x4a, 0x41, 0x45, 0x4a, 0x75,
  0x59, 0x57, 0x31, 0x6c, 0x41, 0x41, 0x41, 0x45, 0x46, 0x41, 0x41, 0x41,
  0x41, 0x55, 0x55, 0x41, 0x41, 0x41, 0x46, 0x46, 0x56, 0x78, 0x6d, 0x6d,
  0x37, 0x6e, 0x42, 0x76, 0x63, 0x33, 0x51, 0x41, 0x41, 0x41, 0x56, 0x63,
  0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x43, 0x41, 0x41,
  0x41, 0x77, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x45, 0x41, 0x41, 0x47, 0x51,
  0x41, 0x41, 0x55, 0x41, 0x41, 0x41, 0x4b, 0x5a, 0x41, 0x73, 0x77, 0x41,
  0x41, 0x41, 0x43, 0x50, 0x41, 0x70, 0x6b, 0x43, 0x7a, 0x41, 0x41, 0x41,
  0x41, 0x65, 0x73, 0x41, 0x4d, 0x77, 0x45, 0x4a, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x52, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x38, 0x47, 0x59, 0x44,
  0x77, 0x50, 0x2f, 0x41, 0x41, 0x45, 0x41, 0x44, 0x77, 0x41, 0x42, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x67, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x41, 0x41, 0x55, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x41, 0x41,
  0x41, 0x42, 0x51, 0x41, 0x42, 0x41, 0x41, 0x34, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x67, 0x41, 0x49, 0x41, 0x41, 0x49, 0x41, 0x41, 0x67, 0x41, 0x42,
  0x41, 0x43, 0x44, 0x77, 0x5a, 0x76, 0x2f, 0x39, 0x2f, 0x2f, 0x38, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x44, 0x77, 0x5a, 0x66, 0x2f, 0x39,
  0x2f, 0x2f, 0x38, 0x41, 0x41, 0x66, 0x2f, 0x6a, 0x44, 0x35, 0x38, 0x41,
  0x41, 0x77, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x41, 0x41, 0x48, 0x2f,
  0x2f, 0x77, 0x41, 0x50, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x41, 0x41, 0x41, 0x33,
  0x4f, 0x51, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41,
  0x41, 0x44, 0x63, 0x35, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x67, 0x41, 0x41, 0x4e, 0x7a, 0x6b, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x32, 0x34, 0x44,
  0x62, 0x67, 0x41, 0x66, 0x41, 0x44, 0x38, 0x41, 0x41, 0x41, 0x45, 0x55,
  0x44, 0x77, 0x45, 0x58, 0x46, 0x68, 0x55, 0x55, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x53, 0x49, 0x6e, 0x4a, 0x6a, 0x55, 0x52, 0x4e, 0x44, 0x63, 0x32,
  0x4d, 0x7a, 0x49, 0x66, 0x41, 0x54, 0x63, 0x32, 0x4d, 0x7a, 0x49, 0x66,
  0x41, 0x52, 0x59, 0x56, 0x41, 0x52, 0x45, 0x55, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x69, 0x38, 0x42, 0x42, 0x77, 0x59, 0x6a, 0x49, 0x69, 0x38, 0x42,
  0x4a, 0x6a, 0x55, 0x30, 0x50, 0x77, 0x45, 0x6e, 0x4a, 0x6a, 0x55, 0x30,
  0x4e, 0x7a, 0x59, 0x7a, 0x49, 0x54, 0x49, 0x58, 0x46, 0x68, 0x55, 0x42,
  0x72, 0x77, 0x57, 0x2b, 0x55, 0x67, 0x73, 0x4c, 0x43, 0x77, 0x37, 0x2f,
  0x41, 0x41, 0x38, 0x4c, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x34, 0x4c,
  0x55, 0x37, 0x30, 0x47, 0x42, 0x77, 0x67, 0x47, 0x51, 0x51, 0x55, 0x42,
  0x76, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x38, 0x4c, 0x55, 0x72, 0x34, 0x46,
  0x43, 0x41, 0x63, 0x47, 0x51, 0x51, 0x59, 0x47, 0x76, 0x6c, 0x4d, 0x4b,
  0x43, 0x67, 0x73, 0x50, 0x41, 0x51, 0x41, 0x50, 0x43, 0x77, 0x73, 0x42,
  0x57, 0x77, 0x63, 0x47, 0x76, 0x56, 0x4d, 0x4c, 0x44, 0x67, 0x38, 0x4c,
  0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x45, 0x41, 0x44, 0x67, 0x73, 0x4c,
  0x43, 0x31, 0x4b, 0x2b, 0x42, 0x51, 0x56, 0x42, 0x42, 0x67, 0x67, 0x42,
  0x37, 0x76, 0x38, 0x41, 0x44, 0x77, 0x73, 0x4b, 0x43, 0x6c, 0x4f, 0x2b,
  0x42, 0x67, 0x5a, 0x42, 0x42, 0x67, 0x63, 0x49, 0x42, 0x62, 0x35, 0x53,
  0x43, 0x77, 0x38, 0x50, 0x43, 0x77, 0x73, 0x4c, 0x43, 0x77, 0x38, 0x41,
  0x41, 0x67, 0x41, 0x48, 0x41, 0x41, 0x63, 0x44, 0x5a, 0x67, 0x4e, 0x6d,
  0x41, 0x42, 0x38, 0x41, 0x50, 0x77, 0x41, 0x41, 0x41, 0x52, 0x45, 0x55,
  0x42, 0x77, 0x59, 0x6a, 0x49, 0x69, 0x38, 0x42, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x69, 0x38, 0x42, 0x4a, 0x6a, 0x55, 0x30, 0x50, 0x77, 0x45, 0x6e,
  0x4a, 0x6a, 0x55, 0x30, 0x4e, 0x7a, 0x59, 0x7a, 0x49, 0x54, 0x49, 0x58,
  0x46, 0x68, 0x55, 0x42, 0x46, 0x41, 0x38, 0x42, 0x46, 0x78, 0x59, 0x56,
  0x46, 0x41, 0x63, 0x47, 0x49, 0x79, 0x45, 0x69, 0x4a, 0x79, 0x59, 0x31,
  0x45, 0x54, 0x51, 0x33, 0x4e, 0x6a, 0x4d, 0x79, 0x48, 0x77, 0x45, 0x33,
  0x4e, 0x6a, 0x4d, 0x79, 0x48, 0x77, 0x45, 0x57, 0x46, 0x51, 0x47, 0x33,
  0x43, 0x77, 0x73, 0x50, 0x44, 0x77, 0x70, 0x54, 0x76, 0x51, 0x59, 0x49,
  0x42, 0x77, 0x5a, 0x42, 0x42, 0x67, 0x61, 0x2b, 0x55, 0x67, 0x73, 0x4c,
  0x43, 0x67, 0x38, 0x42, 0x41, 0x41, 0x38, 0x4c, 0x43, 0x77, 0x47, 0x76,
  0x42, 0x62, 0x35, 0x53, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x2f, 0x38, 0x41,
  0x44, 0x67, 0x73, 0x4c, 0x43, 0x77, 0x73, 0x4f, 0x44, 0x77, 0x74, 0x53,
  0x76, 0x67, 0x59, 0x48, 0x43, 0x41, 0x56, 0x43, 0x42, 0x51, 0x47, 0x53,
  0x2f, 0x77, 0x41, 0x50, 0x43, 0x67, 0x73, 0x4c, 0x55, 0x72, 0x34, 0x47,
  0x42, 0x6b, 0x45, 0x47, 0x42, 0x77, 0x67, 0x47, 0x76, 0x56, 0x4d, 0x4b,
  0x44, 0x77, 0x38, 0x4c, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x47, 0x41,
  0x42, 0x77, 0x61, 0x2b, 0x55, 0x67, 0x73, 0x50, 0x44, 0x67, 0x73, 0x4c,
  0x43, 0x77, 0x73, 0x4f, 0x41, 0x51, 0x41, 0x50, 0x43, 0x77, 0x73, 0x4c,
  0x55, 0x72, 0x34, 0x46, 0x42, 0x55, 0x49, 0x46, 0x43, 0x41, 0x41, 0x42,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x6d, 0x6a, 0x47, 0x6e,
  0x66, 0x46, 0x38, 0x50, 0x50, 0x50, 0x55, 0x41, 0x43, 0x77, 0x51, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x4e, 0x45, 0x46, 0x64, 0x68, 0x63, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x30, 0x51, 0x56, 0x32, 0x46, 0x77, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x44, 0x62, 0x67, 0x4e, 0x75, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x41, 0x41, 0x43, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x50, 0x41, 0x2f, 0x38, 0x41, 0x41,
  0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x4e, 0x75,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x47,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x67, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41,
  0x41, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x48, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x4b, 0x41, 0x42, 0x51, 0x41, 0x48, 0x67, 0x42, 0x38,
  0x41, 0x4e, 0x6f, 0x41, 0x41, 0x41, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x42, 0x67, 0x42, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x44, 0x67, 0x43, 0x75, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x45, 0x41, 0x44, 0x67, 0x41, 0x41,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41,
  0x44, 0x67, 0x42, 0x48, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x4d, 0x41, 0x44, 0x67, 0x41, 0x6b, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x44, 0x67, 0x42, 0x56,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x55, 0x41,
  0x46, 0x67, 0x41, 0x4f, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x59, 0x41, 0x42, 0x77, 0x41, 0x79, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x6f, 0x41, 0x4e, 0x41, 0x42, 0x6a,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x45, 0x41,
  0x44, 0x67, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a,
  0x41, 0x41, 0x49, 0x41, 0x44, 0x67, 0x42, 0x48, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x4d, 0x41, 0x44, 0x67, 0x41, 0x6b,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x51, 0x41,
  0x44, 0x67, 0x42, 0x56, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a,
  0x41, 0x41, 0x55, 0x41, 0x46, 0x67, 0x41, 0x4f, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x59, 0x41, 0x44, 0x67, 0x41, 0x35,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x6f, 0x41,
  0x4e, 0x41, 0x42, 0x6a, 0x41, 0x47, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x34, 0x41,
  0x56, 0x67, 0x42, 0x6c, 0x41, 0x48, 0x49, 0x41, 0x63, 0x77, 0x42, 0x70,
  0x41, 0x47, 0x38, 0x41, 0x62, 0x67, 0x41, 0x67, 0x41, 0x44, 0x45, 0x41,
  0x4c, 0x67, 0x41, 0x77, 0x41, 0x47, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x35, 0x70,
  0x59, 0x32, 0x39, 0x74, 0x62, 0x32, 0x39, 0x75, 0x41, 0x47, 0x6b, 0x41,
  0x59, 0x77, 0x42, 0x76, 0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x34, 0x41, 0x55, 0x67, 0x42, 0x6c, 0x41, 0x47, 0x63, 0x41,
  0x64, 0x51, 0x42, 0x73, 0x41, 0x47, 0x45, 0x41, 0x63, 0x67, 0x42, 0x70,
  0x41, 0x47, 0x4d, 0x41, 0x62, 0x77, 0x42, 0x74, 0x41, 0x47, 0x38, 0x41,
  0x62, 0x77, 0x42, 0x75, 0x41, 0x45, 0x59, 0x41, 0x62, 0x77, 0x42, 0x75,
  0x41, 0x48, 0x51, 0x41, 0x49, 0x41, 0x42, 0x6e, 0x41, 0x47, 0x55, 0x41,
  0x62, 0x67, 0x42, 0x6c, 0x41, 0x48, 0x49, 0x41, 0x59, 0x51, 0x42, 0x30,
  0x41, 0x47, 0x55, 0x41, 0x5a, 0x41, 0x41, 0x67, 0x41, 0x47, 0x49, 0x41,
  0x65, 0x51, 0x41, 0x67, 0x41, 0x45, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x45, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x34, 0x41,
  0x4c, 0x67, 0x41, 0x41, 0x41, 0x41, 0x41, 0x44, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x0a
};

static void
print_html_header (FILE * fp, char *now)
{
  fprintf (fp, "<!DOCTYPE html>\n");
  fprintf (fp, "<html lang=\"en\"><head>\n");
  fprintf (fp, "<title>Server Statistics - %s</title>\n", now);
  fprintf (fp, "<meta charset=\"UTF-8\" />");
  fprintf (fp, "<meta name=\"robots\" content=\"noindex, nofollow\" />");

  fprintf (fp, "<script type=\"text/javascript\">\n");
  fprintf (fp, "function t(c){for(var b=c.parentNode.parentNode.parentNode");
  fprintf (fp, ".parentNode.getElementsByTagName('tr'),a=0;a<b.length;a++)");
  fprintf (fp, "b[a].classList.contains('hide')?(b[a].classList.add('show'),");
  fprintf (fp, "b[a].classList.remove('hide'),c.classList.remove('icon-expand'),");
  fprintf (fp, "c.classList.add('icon-compress')):b[a].classList.contains('show')&&");
  fprintf (fp, "(b[a].classList.add('hide'),b[a].classList.remove('show'),");
  fprintf (fp, "c.classList.remove('icon-compress'),c.classList.add('icon-expand'))};");


  fprintf (fp, "</script>\n");

  fprintf (fp, "<style type=\"text/css\">");
  fprintf (fp,
  "html {"
  "    font-size: 100%%;"
  "    -ms-text-size-adjust: 100%%;"
  "    -webkit-text-size-adjust: 100%%;"
  "}"
  "html {"
  "    font-family: sans-serif"
  "}"
  "body {"
  "    font-size: 80%%;"
  "    color: #777;"
  "    margin: 0;"
  "}"
  "a:focus {"
  "    outline: thin dotted"
  "}"
  "a:active,"
  "a:hover {"
  "    outline: 0"
  "}"
  "p {"
  "    margin: 0 0 1em 0"
  "}"
  "ul {"
  "    margin: 1em 0"
  "}"
  "ul {"
  "    padding: 0 0 0 40px"
  "}"
  "table {"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "}"
  "h2 {"
  "    font-weight: 700;"
  "    color: #4b4b4b;"
  "    font-size: 1.2em;"
  "    margin: .83em 0 .20em 0;"
  "}"
  ".agent-hide,"
  ".hide {"
  "    display: none"
  "}"
  ".r,"
  ".s {"
  "    cursor: pointer"
  "}"
  ".r {"
  "    float: right"
  "}"
  "thead th {"
  "    text-align: center"
  "}"
  ".max {"
  "    background: #f0ad4e;"
  "    border-radius: 5px;"
  "    color: #FFF;"
  "    padding: 2px 5px;"
  "}"
  ".fr {"
  "    width:100%%;"
  "    text-align:right;"
  "}"
  "#layout {"
  "    padding-left: 225px;"
  "    left: 0;"
  "}"
  ".l-box {"
  "    padding: 0 1.3em 1.3em 1.3em"
  "}"
  ".graph {"
  "    text-align: center;"
  "}"
  ".graph .bar {"
  "    -webkit-box-sizing: border-box;"
  "    -moz-box-sizing: border-box;"
  "    background-color: rgba(119, 119, 119, 0.7);"
  "    border-bottom-right-radius: 3px;"
  "    border-top-right-radius: 3px;"
  "    box-sizing: border-box;"
  "    color: #ffffff;"
  "    height: 17px;"
  "    width: 0;"
  "}"
  ".graph .light {"
  "    background-color: rgba(119, 119, 119, 0.3);"
  "    margin-top: 1px;"
  "}"
  "#menu {"
  "    -webkit-overflow-scroll: touch;"
  "    -webkit-transition: left 0.75s, -webkit-transform 0.75s;"
  "    background: #242424;"
  "    border-right: 1px solid #3E444C;"
  "    bottom: 0;"
  "    box-shadow: inset 0 0 90px #000;"
  "    left: 225px;"
  "    margin-left: -225px;"
  "    outline: 1px solid #101214;"
  "    overflow-y: auto;"
  "    position: fixed;"
  "    text-shadow: 0px -1px 0px #000;"
  "    top: 0;"
  "    transition: left 0.75s, -webkit-transform 0.75s, transform 0.75s;"
  "    width: 225px;"
  "    z-index: 1000;"
  "}"
  "#menu a {"
  "    border: 0;"
  "    border-bottom: 1px solid #111;"
  "    box-shadow: 0 1px 0 #383838;"
  "    color: #999;"
  "    padding: .6em 0 .6em .6em;"
  "    white-space: normal;"
  "}"
  "#menu p {"
  "    color: #eee;"
  "    font-size: 85%%;"
  "    padding: .6em;"
  "    text-shadow: 0 -1px 0 #000;"
  "}"
  "#menu .pure-menu-open {"
  "    background: transparent;"
  "    border: 0;"
  "}"
  "#menu .pure-menu ul {"
  "    border: 0;"
  "    background: transparent;"
  "}"
  "#menu .pure-menu li a:hover,"
  "#menu .pure-menu li a:focus {"
  "    background: #333"
  "}"
  "#menu .pure-menu-heading:hover,"
  "#menu .pure-menu-heading:focus {"
  "    color: #999"
  "}"
  "#menu .pure-menu-heading {"
  "    color: #FFF;"
  "    font-size: 110%%;"
  "    font-weight: bold;"
  "}"
  ".pure-u {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    width: 100%%"
  "}"
  ".pure-g-r {"
  "    letter-spacing: -.31em;"
  "    *letter-spacing: normal;"
  "    *word-spacing: -.43em;"
  "    font-family: sans-serif;"
  "    display: -webkit-flex;"
  "    -webkit-flex-flow: row wrap;"
  "    display: -ms-flexbox;"
  "    -ms-flex-flow: row wrap;"
  "}"
  ".pure-g-r {"
  "    word-spacing: -.43em"
  "}"
  ".pure-g-r [class *=pure-u] {"
  "    font-family: sans-serif"
  "}"
  "@media (max-width:480px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  "@media (max-width:767px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  ".pure-menu ul {"
  "    position: absolute;"
  "    visibility: hidden;"
  "}"
  ".pure-menu.pure-menu-open {"
  "    visibility: visible;"
  "    z-index: 2;"
  "    width: 100%%;"
  "}"
  ".pure-menu ul {"
  "    left: -10000px;"
  "    list-style: none;"
  "    margin: 0;"
  "    padding: 0;"
  "    top: -10000px;"
  "    z-index: 1;"
  "}"
  ".pure-menu>ul {"
  "    position: relative"
  "}"
  ".pure-menu-open>ul {"
  "    left: 0;"
  "    top: 0;"
  "    visibility: visible;"
  "}"
  ".pure-menu-open>ul:focus {"
  "    outline: 0"
  "}"
  ".pure-menu li {"
  "    position: relative"
  "}"
  ".pure-menu a,"
  ".pure-menu .pure-menu-heading {"
  "    display: block;"
  "    color: inherit;"
  "    line-height: 1.5em;"
  "    padding: 5px 20px;"
  "    text-decoration: none;"
  "    white-space: nowrap;"
  "}"
  ".pure-menu li a {"
  "    padding: 5px 20px"
  "}"
  ".pure-menu.pure-menu-open {"
  "    background: #fff;"
  "    border: 1px solid #b7b7b7;"
  "}"
  ".pure-menu a {"
  "    border: 1px solid transparent;"
  "    border-left: 0;"
  "    border-right: 0;"
  "}"
  ".pure-menu a {"
  "    color: #777"
  "}"
  ".pure-menu li a:hover,"
  ".pure-menu li a:focus {"
  "    background: #eee"
  "}"
  ".pure-menu .pure-menu-heading {"
  "    color: #565d64;"
  "    font-size: 90%%;"
  "    margin-top: .5em;"
  "    border-bottom-width: 1px;"
  "    border-bottom-style: solid;"
  "    border-bottom-color: #dfdfdf;"
  "}"
  ".pure-table {"
  "    animation: float 5s infinite;"
  "    border: 1px solid #cbcbcb;"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "    box-shadow: 0 5px 10px rgba(0, 0, 0, 0.1);"
  "    empty-cells: show;"
  "    border-radius:3px;"
  "}"
  ".pure-table td,"
  ".pure-table th {"
  "    border-left: 1px solid #cbcbcb;"
  "    border-width: 0 0 0 1px;"
  "    font-size: inherit;"
  "    margin: 0;"
  "    overflow: visible;"
  "    padding: 6px 12px;"
  "}"
  ".pure-table th:last-child {"
  "    padding-right: 0;"
  "}"
  ".pure-table th:last-child span {"
  "    margin: 1px 5px 0 15px;"
  "}"
  ".pure-table th {"
  "    border-bottom:4px solid #9ea7af;"
  "    border-right: 1px solid #343a45;"
  "}"
  ".pure-table td:first-child,"
  ".pure-table th:first-child {"
  "    border-left-width: 0"
  "}"
  ".pure-table td:last-child {"
  "    white-space: normal;"
  "    width: auto;"
  "    word-break: break-all;"
  "    word-wrap: break-word;"
  "}"
  ".pure-table thead {"
  "    background: #242424;"
  "    color: #FFF;"
  "    text-align: left;"
  "    text-shadow: 0px -1px 0px #000;"
  "    vertical-align: bottom;"
  "}"
  ".pure-table td {"
  "    background-color: #FFF"
  "}"
  ".pure-table td.num {"
  "    text-align: right"
  "}"
  ".pure-table .sub td {"
  "    background-color: #F2F2F2;"
  "}"
  ".pure-table tbody tr:hover,"
  ".pure-table-striped tr:nth-child(2n-1) td {"
  "    background-color: #f4f4f4"
  "}"
  ".pure-table tr {"
  "    border-bottom: 1px solid #ddd;"
  "}"
  ".grid {"
  "    background: white;"
  "    margin: 0 0 20px 0;"
  "}"
  ".grid * {"
  "    -moz-box-sizing: border-box;"
  "    -webkit-box-sizing: border-box;"
  "    box-sizing: border-box;"
  "}"
  ".grid:after {"
  "    content:\"\";"
  "    display: table;"
  "    clear: both;"
  "}"
  "[class*='col-'] {"
  "    float: left;"
  "    padding-right: 20px;"
  "}"
  ".grid[class*='col-']:last-of-type {"
  "    padding-right: 0;"
  "}"
  ".col-1-3 {"
  "    width: 33.33%%;"
  "}"
  ".col-1-2 {"
  "    width: 50%%;"
  "}"
  ".col-1-4 {"
  "    width: 25%%;"
  "}"
  ".col-1-6 {"
  "    width: 16.6%%;"
  "}"
  ".col-1-8 {"
  "    width: 12.5%%;"
  "}"
  ".grid-module {"
  "    border-top: 1px solid #9ea7af;"
  "}"
  ".col-title {"
  "    color: #4B4B4B;"
  "    font-weight: 700;"
  "    margin: 2px 0 10px;"
  "    width: 100%%;"
  "}"
  ".label {"
  "    background-color: #9D9D9D;"
  "    border-radius: .25em;"
  "    color: #fff;"
  "    display: inline;"
  "    font-weight: 700;"
  "    line-height: 1;"
  "    padding: .2em .6em .3em;"
  "    text-align: center;"
  "    vertical-align: baseline;"
  "    white-space: nowrap;"
  "}"
  ".green {"
  "    background: #5cb85c;"
  "}"
  ".red {"
  "    background: #d9534f;"
  "}"
  ".trunc {"
  "    width: 100%%;"
  "    white-space: nowrap;"
  "    overflow: hidden;"
  "    text-overflow: ellipsis;"
  "}"
  "@font-face {"
  "    font-family: 'icomoon';"
  "    src: url(data:application/font-woff;charset=utf-8;base64,%s) format('woff');"
  "    font-weight: normal;"
  "    font-style: normal;"
  "}"
  "[class^=\"icon-\"], [class*=\" icon-\"] {"
  "    font-family: 'icomoon';"
  "    speak: none;"
  "    font-style: normal;"
  "    font-weight: normal;"
  "    font-variant: normal;"
  "    text-transform: none;"
  "    line-height: 1;"
  "    -webkit-font-smoothing: antialiased;"
  "    -moz-osx-font-smoothing: grayscale;"
  "}"
  ".icon-expand:before {"
  "    content: '\\f065';"
  "}"
  ".icon-compress:before {"
  "    content: '\\f066';"
  "}"
  "@media (max-width: 974px) {"
  "    #layout {"
  "        position: relative;"
  "        padding-left: 0;"
  "    }"
  "    #layout.active {"
  "        position: relative;"
  "        left: 200px;"
  "    }"
  "    #layout.active #menu {"
  "        left: 200px;"
  "        width: 200px;"
  "    }"
  "    #menu {"
  "        left: 0"
  "    }"
  "    .pure-menu-link {"
  "        position: fixed;"
  "        left: 0;"
  "        display: block;"
  "    }"
  "    #layout.active .pure-menu-link {"
  "        left: 200px"
  "    }"
  "}", icons);

  fprintf (fp, "</style>\n");
  fprintf (fp, "</head>\n");
  fprintf (fp, "<body>\n");

  fprintf (fp, "<div class=\"pure-g-r\" id=\"layout\">");
}

/* *INDENT-ON* */

static GOutput *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

static int
get_max_visitor (GHolder * h)
{
  int i, max = 0;
  for (i = 0; i < h->idx; i++) {
    int cur = h->items[i].metrics->visitors;
    if (cur > max)
      max = cur;
  }

  return max;
}

static int
get_max_hit (GHolder * h)
{
  int i, max = 0;
  for (i = 0; i < h->idx; i++) {
    int cur = h->items[i].metrics->hits;
    if (cur > max)
      max = cur;
  }

  return max;
}

/* sanitize output with html entities for special chars */
static void
clean_output (FILE * fp, char *s)
{
  while (*s) {
    switch (*s) {
    case '\'':
      fprintf (fp, "&#39;");
      break;
    case '"':
      fprintf (fp, "&#34;");
      break;
    case '&':
      fprintf (fp, "&amp;");
      break;
    case '<':
      fprintf (fp, "&lt;");
      break;
    case '>':
      fprintf (fp, "&gt;");
      break;
    case ' ':
      fprintf (fp, "&nbsp;");
      break;
    default:
      fputc (*s, fp);
      break;
    }
    s++;
  }
}


static void
print_pure_menu (FILE * fp, char *now)
{

  fprintf (fp, "<div id=\"menu\" class=\"pure-u\">");
  fprintf (fp, "<div class=\"pure-menu pure-menu-open\">");
  fprintf (fp, "<a class=\"pure-menu-heading\" href=\"%s\">", GO_WEBSITE);
  fprintf (fp, "<img src='data:image/png;base64,%s'/>", GO_LOGO);
  fprintf (fp, "</a>");
  fprintf (fp, "<ul>");
  fprintf (fp, "<li><a href=\"#\">Overall</a></li>");
  fprintf (fp, "<li><a href=\"#%s\">Unique visitors</a></li>", VISIT_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested files</a></li>", REQUE_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested static files</a></li>", STATI_ID);
  fprintf (fp, "<li><a href=\"#%s\">Not found URLs</a></li>", FOUND_ID);
  fprintf (fp, "<li><a href=\"#%s\">Hosts</a></li>", HOSTS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Operating Systems</a></li>", OPERA_ID);
  fprintf (fp, "<li><a href=\"#%s\">Browsers</a></li>", BROWS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referrers URLs</a></li>", REFER_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referring sites</a></li>", SITES_ID);
  fprintf (fp, "<li><a href=\"#%s\">Keyphrases</a></li>", KEYPH_ID);
#ifdef HAVE_LIBGEOIP
  fprintf (fp, "<li><a href=\"#%s\">Geo Location</a></li>", GEOLO_ID);
#endif
  fprintf (fp, "<li><a href=\"#%s\">Status codes</a></li>", CODES_ID);
  fprintf (fp, "<li class=\"menu-item-divided\"></li>");

  fprintf (fp, "</ul>");
  fprintf (fp, "<p>Generated by<br />GoAccess %s<br />—<br />%s</p>",
           GO_VERSION, now);
  fprintf (fp, "</div>");
  fprintf (fp, "</div> <!-- menu -->");

  fprintf (fp, "<div id=\"main\" class=\"pure-u-1\">");
  fprintf (fp, "<div class=\"l-box\">");
}

static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "</div> <!-- l-box -->\n");
  fprintf (fp, "</div> <!-- main -->\n");
  fprintf (fp, "</div> <!-- layout -->\n");
  fprintf (fp, "</body>\n");
  fprintf (fp, "</html>");
}

static void
print_html_h2 (FILE * fp, const char *title, const char *id)
{
  if (id)
    fprintf (fp, "<h2 id=\"%s\">%s</h2>", id, title);
  else
    fprintf (fp, "<h2>%s</h2>", title);
}

static void
print_p (FILE * fp, const char *paragraph)
{
  fprintf (fp, "<p>%s</p>", paragraph);
}

static void
print_html_begin_table (FILE * fp)
{
  fprintf (fp, "<table class=\"pure-table\">\n");
}

static void
print_html_end_table (FILE * fp)
{
  fprintf (fp, "</table>\n");
}

static void
print_html_begin_thead (FILE * fp)
{
  fprintf (fp, "<thead>\n");
}

static void
print_html_end_thead (FILE * fp)
{
  fprintf (fp, "</thead>\n");
}

static void
print_html_begin_tbody (FILE * fp)
{
  fprintf (fp, "<tbody>\n");
}

static void
print_html_end_tbody (FILE * fp)
{
  fprintf (fp, "</tbody>\n");
}

static void
print_html_begin_tr (FILE * fp, int hide, int sub)
{
  if (hide)
    fprintf (fp, "<tr class='hide %s'>", (sub ? "sub" : "root"));
  else
    fprintf (fp, "<tr class='%s'>", (sub ? "sub" : "root"));
}

static void
print_html_end_tr (FILE * fp)
{
  fprintf (fp, "</tr>");
}

static void
print_html_end_div (FILE * fp)
{
  fprintf (fp, "</div>");
}

static void
print_html_begin_grid (FILE * fp)
{
  fprintf (fp, "<div class='grid grid-pad'>");
}

static void
print_html_begin_grid_col (FILE * fp, int size)
{
  fprintf (fp, "<div class='col-1-%d'>", size);
}

static void
print_html_begin_grid_module (FILE * fp)
{
  fprintf (fp, "<div class='grid-module'>");
}

static void
print_html_begin_col_wrap (FILE * fp, int size)
{
  print_html_begin_grid_col (fp, size);
  print_html_begin_grid_module (fp);
}

static void
print_html_end_col_wrap (FILE * fp)
{
  print_html_end_div (fp);
  print_html_end_div (fp);
}

static void
print_html_col_title (FILE * fp, const char *title)
{
  fprintf (fp, "<div class='col-title trunc'>%s</div>", title);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
print_graph (FILE * fp, int max_hit, int max_vis, int hits, int visitors)
{
  const char *s, *c = "class='bar'";
  float lh = get_percentage (max_hit, hits), lv = 0;
  int h = 0;

  setlocale (LC_NUMERIC, "POSIX");
  fprintf (fp, "<td class='graph'>");

  h = max_vis ? 8 : 16;
  lh = lh < 1 ? 1 : lh;

  s = "<div title='Hits:%d%%' %s style='width:%f%%;height:%dpx'></div>";
  fprintf (fp, s, (int) lh, c, lh, h);

  if (max_vis) {
    c = "class='bar light'";
    s = "<div title='Visitors: %d%%' %s style='width:%f%%;height:%dpx'></div>";
    lv = get_percentage (max_vis, visitors);
    lv = lv < 1 ? 1 : lv;
    fprintf (fp, s, (int) lv, c, lv, h);
  }

  fprintf (fp, "</td>\n");
  setlocale (LC_NUMERIC, "");
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

static void
print_table_head (FILE * fp, GModule module)
{
  print_html_h2 (fp, module_to_head (module), module_to_id (module));
  print_p (fp, module_to_desc (module));
}

static void
print_metric_hits (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td class='num'>%'d</td>", nmetrics->hits);
}

static void
print_metric_visitors (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td class='num'>%'d</td>", nmetrics->visitors);
}

static void
print_metric_percent (FILE * fp, GMetrics * nmetrics, int max_hit)
{
  fprintf (fp, "<td class='num'>");
  fprintf (fp, "<span class='%s'>%4.2f%%</span>", (max_hit ? "max" : ""),
           nmetrics->percent);
  fprintf (fp, "</td>");
}

static void
print_metric_bw (FILE * fp, GMetrics * nmetrics)
{
  char *bw = filesize_str (nmetrics->bw.nbw);

  fprintf (fp, "<td class='num'>");
  clean_output (fp, bw);
  fprintf (fp, "</td>");

  free (bw);
}

static void
print_metric_avgts (FILE * fp, GMetrics * nmetrics)
{
  char *ts = NULL;
  if (!conf.serve_usecs)
    return;

  ts = usecs_to_str (nmetrics->avgts.nts);
  fprintf (fp, "<td class='num'>");
  clean_output (fp, ts);
  fprintf (fp, "</td>");

  free (ts);
}

static void
print_metric_data (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->data);
  fprintf (fp, "</td>");
}

static void
print_metric_protocol (FILE * fp, GMetrics * nmetrics)
{
  if (!conf.append_protocol)
    return;

  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->protocol);
  fprintf (fp, "</td>");
}

static void
print_metric_method (FILE * fp, GMetrics * nmetrics)
{
  if (!conf.append_method)
    return;

  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->method);
  fprintf (fp, "</td>");
}

static void
print_metrics (FILE * fp, GMetrics * nmetrics, int max_hit, int max_vis,
               int sub, const GOutput * panel)
{
  if (panel->visitors)
    print_metric_visitors (fp, nmetrics);
  if (panel->hits)
    print_metric_hits (fp, nmetrics);
  if (panel->percent)
    print_metric_percent (fp, nmetrics, max_hit == nmetrics->hits);
  if (panel->bw)
    print_metric_bw (fp, nmetrics);
  if (panel->avgts)
    print_metric_avgts (fp, nmetrics);
  if (panel->protocol)
    print_metric_protocol (fp, nmetrics);
  if (panel->method)
    print_metric_method (fp, nmetrics);
  if (panel->data)
    print_metric_data (fp, nmetrics);

  if (panel->graph && max_hit && !panel->sub_graph && sub)
    fprintf (fp, "<td></td>");
  else if (panel->graph && max_hit)
    print_graph (fp, max_hit, max_vis, nmetrics->hits, nmetrics->visitors);
}

static void
print_html_sub_items (FILE * fp, GHolder * h, int idx, int processed,
                      int max_hit, int max_vis, const GOutput * panel)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  GSubList *sub_list = h->items[idx].sub_list;
  int i = 0;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, processed);

    print_html_begin_tr (fp, 1, 1);
    print_metrics (fp, nmetrics, max_hit, max_vis, 1, panel);
    print_html_end_tr (fp);

    free (nmetrics);
  }
}

static void
print_html_data (FILE * fp, GHolder * h, int processed, int max_hit,
                 int max_vis, const GOutput * panel)
{
  GMetrics *nmetrics;
  int i;

  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, processed);

    print_html_begin_tr (fp, (i > OUTPUT_N), 0);
    print_metrics (fp, nmetrics, max_hit, max_vis, 0, panel);
    print_html_end_tr (fp);

    if (h->sub_items_size)
      print_html_sub_items (fp, h, i, processed, max_hit, max_vis, panel);

    free (nmetrics);
  }
}

static void
print_html_visitors (FILE * fp, GHolder * h, int processed,
                     const GOutput * panel)
{
  int max_hit = 0, max_vis = 0;

  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  fprintf (fp, "<th>Date</th>");
  fprintf (fp,
           "<th class='fr'>&nbsp;<span class='r icon-expand' onclick='t(this)'></span></th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  max_hit = get_max_hit (h);
  max_vis = get_max_visitor (h);
  print_html_data (fp, h, processed, max_hit, max_vis, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_requests (FILE * fp, GHolder * h, int processed,
                     const GOutput * panel)
{
  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  if (conf.append_protocol)
    fprintf (fp, "<th>Protocol</th>");
  if (conf.append_method)
    fprintf (fp, "<th>Method</th>");
  fprintf (fp, "<th>");
  fprintf (fp, "Request <span class='r icon-expand' onclick='t(this)'></span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  print_html_data (fp, h, processed, 0, 0, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_common (FILE * fp, GHolder * h, int processed, const GOutput * panel)
{
  int max_hit = 0, max_vis = 0;
  const char *lbl = module_to_label (h->module);

  if (panel->graph) {
    max_hit = get_max_hit (h);
    max_vis = get_max_visitor (h);
  }

  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  if (max_hit)
    fprintf (fp, "<th>%s</th>", lbl);
  fprintf (fp, "<th class='%s'>%s", max_hit ? "fr" : "", max_hit ? "" : lbl);
  fprintf (fp, "<span class='r icon-expand' onclick='t(this)'>&#8199;</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  print_html_data (fp, h, processed, max_hit, max_vis, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_summary (FILE * fp, GLog * logger)
{
  long long time = 0LL;
  int total = 0;
  off_t log_size = 0;
  char *bw, *size;

  print_html_h2 (fp, T_HEAD, GENER_ID);
  print_html_begin_grid (fp);

  /* total requests */
  total = logger->process;
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_REQUESTS);
  fprintf (fp, "<h3 class='label green'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* invalid requests */
  total = logger->invalid;
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_FAILED);
  fprintf (fp, "<h3 class='label red'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* generated time */
  time = (long long) end_proc - start_proc;
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_GEN_TIME);
  fprintf (fp, "<h3 class='label'>%llu secs</h3>", time);
  print_html_end_col_wrap (fp);

  total = get_ht_size_by_metric (VISITORS, MTRC_UNIQMAP);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_UNIQUE_VIS);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* files */
  total = get_ht_size_by_metric (REQUESTS, MTRC_DATAMAP);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_UNIQUE_FIL);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* excluded hits */
  total = logger->exclude_ip;
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_EXCLUDE_IP);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  print_html_end_div (fp);

  print_html_begin_grid (fp);

  /* referrers */
  total = get_ht_size_by_metric (REFERRERS, MTRC_DATAMAP);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_REFERRER);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* not found */
  total = get_ht_size_by_metric (NOT_FOUND, MTRC_DATAMAP);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_UNIQUE404);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  /* static files */
  total = get_ht_size_by_metric (REQUESTS_STATIC, MTRC_DATAMAP);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_STATIC_FIL);
  fprintf (fp, "<h3 class='label'>%'d</h3>", total);
  print_html_end_col_wrap (fp);

  if (!logger->piping) {
    log_size = file_size (conf.ifile);
    size = filesize_str (log_size);
  } else {
    size = alloc_string ("N/A");
  }

  /* log size */
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_LOG);
  fprintf (fp, "<h3 class='label'>%s</h3>", size);
  print_html_end_col_wrap (fp);

  /* bandwidth */
  bw = filesize_str ((float) logger->resp_size);
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_BW);
  fprintf (fp, "<h3 class='label'>%s</h3>", bw);
  print_html_end_col_wrap (fp);

  /* log path */
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";
  print_html_begin_col_wrap (fp, 6);
  print_html_col_title (fp, T_LOG_PATH);
  fprintf (fp, "<h3 class='trunc' style='color:#242424'>%s</h3>", conf.ifile);
  print_html_end_col_wrap (fp);

  print_html_end_div (fp);

  free (bw);
  free (size);
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * logger, GHolder * holder)
{
  GModule module;
  char now[DATE_TIME];
  FILE *fp = stdout;

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  setlocale (LC_NUMERIC, "");
  print_html_header (fp, now);
  print_pure_menu (fp, now);

  print_html_summary (fp, logger);
  for (module = 0; module < TOTAL_MODULES; module++) {
    const GOutput *panel = panel_lookup (module);
    if (!panel)
      continue;
    panel->render (fp, holder + module, logger->process, panel);
  }

  print_html_footer (fp);

  fclose (fp);
}
