/*
 * Copyright (C) 2000 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pixops.h"

static GTimeVal start_time;

static void 
start_timing (void)
{
  g_get_current_time (&start_time);
}

static double
stop_timing (const char *test, int iterations, int bytes)
{
  GTimeVal stop_time;
  double msecs;
  
  g_get_current_time (&stop_time);
  if (stop_time.tv_usec < start_time.tv_usec)
    {
      stop_time.tv_usec += 1000000;
      stop_time.tv_sec -= 1;
    }

  msecs = (stop_time.tv_sec - start_time.tv_sec) * 1000. +
          (stop_time.tv_usec - start_time.tv_usec) / 1000.;

  printf("%s%d\t%.1f\t\t%.2f\t\t%.2f\n",
	 test, iterations, msecs, msecs / iterations, ((double)bytes * iterations) / (1000*msecs));

  return ((double)bytes * iterations) / (1000*msecs);
}

static void
init_array (double times[3][3][4])
{
  int i, j, k;
  
  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      for (k=0; k<4; k++)
	times[i][j][k] = -1;
}

static void
dump_array (double times[3][3][4])
{
  int i, j;
  
  printf("        3\t4\t4a\n");
  for (i=0; i<3; i++)
    {
      for (j=0; j<4; j++)
	{
	  if (j == 0)
	    switch (i)
	      {
	      case 0:
		printf("3  ");
		break;
	      case 1:
		printf("4  ");
		break;
	      case 2:
		printf("4a ");
		break;
	      }
	  else
	    printf("   ");

	  printf("%6.2f  %6.2f   %6.2f",
		 times[i][0][j], times[i][1][j], times[i][2][j]);

	  switch (j)
	    {
	    case PIXOPS_INTERP_NEAREST:
	      printf ("  NEAREST\n");
	      break;
	    case PIXOPS_INTERP_TILES:
	      printf ("  TILES\n");
	      break;
	    case PIXOPS_INTERP_BILINEAR:
	      printf ("  BILINEAR\n");
	      break;
	    case PIXOPS_INTERP_HYPER:
	      printf ("  HYPER\n");
	      break;
	    }
	}
    }
  printf("\n");
}

#define ITERS 10

int main (int argc, char **argv)
{
  int src_width, src_height, dest_width, dest_height;
  unsigned char *src_buf, *dest_buf;
  int src_index, dest_index;
  int i;
  double scale_times[3][3][4];
  double composite_times[3][3][4];
  double composite_color_times[3][3][4];

  if (argc == 5)
    {
      src_width = atoi(argv[1]);
      src_height = atoi(argv[2]);
      dest_width = atoi(argv[3]);
      dest_height = atoi(argv[4]);
    }
  else if (argc == 1)
    {
      src_width = 343;
      src_height = 343;
      dest_width = 711;
      dest_height = 711;
    }
  else
    {
      fprintf (stderr, "Usage: scale [src_width src_height dest_width dest_height]\n");
      exit(1);
    }


  printf ("Scaling from (%d, %d) to (%d, %d)\n\n", src_width, src_height, dest_width, dest_height);

  init_array (scale_times);
  init_array (composite_times);
  init_array (composite_color_times);

  for (src_index = 0; src_index < 3; src_index++)
    for (dest_index = 0; dest_index < 3; dest_index++)
      {
	int src_channels = (src_index == 0) ? 3 : 4;
	int src_has_alpha = (src_index == 2);
	int dest_channels = (dest_index == 0) ? 3 : 4;
	int dest_has_alpha = (dest_index == 2);
	
	int src_rowstride = (src_channels*src_width + 3) & ~3;
	int dest_rowstride = (dest_channels *dest_width + 3) & ~3;

	int filter_level;

	src_buf = g_malloc(src_rowstride * src_height);
	memset (src_buf, 0x80, src_rowstride * src_height);
	
	dest_buf = g_malloc(dest_rowstride * dest_height);
	memset (dest_buf, 0x80, dest_rowstride * dest_height);

	for (filter_level = PIXOPS_INTERP_NEAREST ; filter_level <= PIXOPS_INTERP_HYPER; filter_level++)
	  {
	    printf ("src_channels = %d (%s); dest_channels = %d (%s); filter_level=",
		    src_channels, src_has_alpha ? "alpha" : "no alpha",
		    dest_channels, dest_has_alpha ? "alpha" : "no alpha");
	    switch (filter_level)
	      {
	      case PIXOPS_INTERP_NEAREST:
		printf ("PIXOPS_INTERP_NEAREST\n");
		break;
	      case PIXOPS_INTERP_TILES:
		printf ("PIXOPS_INTERP_TILES\n");
		break;
	      case PIXOPS_INTERP_BILINEAR:
		printf ("PIXOPS_INTERP_BILINEAR\n");
		break;
	      case PIXOPS_INTERP_HYPER:
		printf ("PIXOPS_INTERP_HYPER\n");
		break;
	      }

	    printf("\t\t\titers\ttotal\t\tmsecs/iter\tMpixels/sec\t\n");


	    if (!(src_has_alpha && !dest_has_alpha))
	      {
		start_timing ();
		for (i = 0; i < ITERS; i++)
		  {
		    _pixops_scale (dest_buf, dest_width, dest_height,
				   dest_rowstride, dest_channels,
				   dest_has_alpha, src_buf, src_width,
				   src_height, src_rowstride, src_channels,
				   src_has_alpha, 0, 0, 0, 0, 0, 0,
				   (double)dest_width / src_width,
				   (double)dest_height / src_height,
				   filter_level);
		  }
		scale_times[src_index][dest_index][filter_level] =
		  stop_timing ("   scale\t\t", ITERS, dest_height * dest_width);
	      }

	    start_timing ();
	    for (i = 0; i < ITERS; i++)
	      {
		_pixops_composite (dest_buf, dest_width, dest_height,
				   dest_rowstride, dest_channels,
				   dest_has_alpha, src_buf, src_width,
				   src_height, src_rowstride, src_channels,
				   src_has_alpha, 0, 0, 0, 0, 0, 0,
				   (double)dest_width / src_width,
				   (double)dest_height / src_height,
				   filter_level, 255);
	      }
	    composite_times[src_index][dest_index][filter_level] =
	      stop_timing ("   composite\t\t", ITERS,
			   dest_height * dest_width);

	    start_timing ();
	    for (i = 0; i < ITERS; i++)
	      {
		_pixops_composite_color (dest_buf, dest_width, dest_height,
					 dest_rowstride, dest_channels,
					 dest_has_alpha, src_buf, src_width,
					 src_height, src_rowstride,
					 src_channels, src_has_alpha, 0, 0,
					 0, 0, 0, 0,
					 (double)dest_width / src_width,
					 (double)dest_height / src_height,
					 filter_level, 255, 0, 0, 16,
					 0xaaaaaa, 0x555555);
	      }
	    composite_color_times[src_index][dest_index][filter_level] =
	      stop_timing ("   composite color\t", ITERS, dest_height * dest_width);

	    printf ("\n");
	  }
	printf ("\n");

	g_free (src_buf);
	g_free (dest_buf);
      }

  printf ("SCALE\n=====\n\n");
  dump_array (scale_times);

  printf ("COMPOSITE\n=========\n\n");
  dump_array (composite_times);

  printf ("COMPOSITE_COLOR\n===============\n\n");
  dump_array (composite_color_times);
  return 0;
}
