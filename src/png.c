/*
 * Copyright (C) 2007  Sean d'Epagnier   All Rights Reserved.
 *
 * Meteor is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Meteor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include "config.h"

#include "image.h"

#ifdef HAVE_LIBPNG

#include <errno.h>
#include <png.h>

#include "util.h"

Image *
image_load_png (const char *filename)
{
  int             i;            /* Looping var */
  int             bpp;          /* Bytes per pixel */
  int             num_passes;   /* Number of interlace passes in file */
  int             pass;         /* Current pass in file */
  FILE           *fp;           /* File pointer */
  png_structp     pp;           /* PNG read pointer */
  png_infop       info;         /* PNG info pointers */
  unsigned char **pixels;       /* Pixel rows */

  Image *volatile image;        /* Image */

  pp = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info = png_create_info_struct (pp);

  if (setjmp (pp->jmpbuf))
    {
      verbose_printf ("Error while reading '%s'. File corrupted?\n", filename);
      return image;
    }

  /* initialise image here, thus avoiding compiler warnings */
  image = NULL;

  /* Open the file and initialize the PNG read "engine"...  */
  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      verbose_printf ("Could not open '%s' for reading: %s\n", filename, strerror (errno));
      return NULL;
    }
  png_init_io (pp, fp);
  png_read_info (pp, info);

  if (info->bit_depth == 16)
    png_set_strip_16 (pp);

  if (info->color_type == PNG_COLOR_TYPE_GRAY && info->bit_depth < 8)
    png_set_expand (pp);

  /* Expand G+tRNS to GA, RGB+tRNS to RGBA */

  if (info->valid & PNG_INFO_tRNS)
    png_set_expand (pp);

  /*
   * Turn on interlace handling... libpng returns just 1 (ie single pass)
   * if the image is not interlaced
   */

  num_passes = png_set_interlace_handling (pp);

  /* Update the info structures after the transformations take effect */
  png_read_update_info (pp, info);

  image = malloc (sizeof (Image));
  image->width  = info->width;
  image->height = info->height;

  switch (info->color_type)
    {
    case PNG_COLOR_TYPE_RGB:           /* RGB */
      bpp = 3;
      image->type = IMAGE_TYPE_RGB;
      break;

    case PNG_COLOR_TYPE_RGB_ALPHA:     /* RGBA */
      bpp = 4;
      image->type = IMAGE_TYPE_RGBA;
      break;

    case PNG_COLOR_TYPE_GRAY:          /* Grayscale */
      bpp = 1;
      image->type = IMAGE_TYPE_GRAY;
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:    /* Grayscale + alpha */
      bpp = 2;
      image->type = IMAGE_TYPE_GRAYA;
      break;

    default:                   /* Aie! Unknown type */
      verbose_printf ("Unknown color model in PNG file '%s'.\n", filename);
      return NULL;
    }
  image->data = malloc (info->height * info->width * bpp);

  /* png decodes by row... */
  pixels = malloc (sizeof (unsigned char *) * info->height);

  for (i = 0; i < image->height; i++)
    pixels[i] = image->data + info->width * info->channels * i;

  for (pass = 0; pass < num_passes; pass++)
    png_read_rows (pp, pixels, NULL, image->height);

  png_read_end (pp, info);

  png_destroy_read_struct (&pp, &info, NULL);

  free (pixels);
  free (pp);
  free (info);

  fclose (fp);

  return image;
}

int write_png(char *filename, int width, int height, unsigned char *rgbdata)
{
    FILE *outfile;
    if(!strcmp(filename, "-")) {
       strcpy(filename, "stdout");
       outfile = stdout;
    } else
       if(!(outfile = fopen(filename, "wb")))
       {
          warning("Error: Couldn't fopen %s.\n", filename);
          return -1;
       }
    
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 
	(png_voidp) NULL, (png_error_ptr) NULL, (png_error_ptr) NULL);
    
    if (!png_ptr)
	warning("Error: Couldn't create PNG write struct.");
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
	png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
	warning("Error: Couldn't create PNG info struct.");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
       png_destroy_write_struct(&png_ptr, &info_ptr);
       fclose(outfile);
       warning("Error: PNG failed to write.");
    }
    
    png_init_io(png_ptr, outfile);
    
    int bit_depth = 8;
    int color_type = PNG_COLOR_TYPE_RGB;

    png_set_IHDR(png_ptr, info_ptr, width, height, 
		 bit_depth, color_type, PNG_INTERLACE_NONE, 
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_text text_ptr[1];
    text_ptr[0].key = "Software";
    text_ptr[0].text = "orbits simulator";
    text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;

    png_set_text(png_ptr, info_ptr, text_ptr, 1);
    
    png_write_info(png_ptr, info_ptr);

    png_bytep row_pointers[height];
    int i;
    for (i=0; i<height; i++)
	row_pointers[i] = rgbdata + (height - i - 1) * 3 * width;
    
    png_write_image(png_ptr, row_pointers);
    
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    
    fclose(outfile);

    return 0;
}

#else

Image *image_load_png(const char *filename)
{
   warning("Would load image %s, but was not compiled "
           "with image support\n", filename);
   return NULL;
}

int write_png(char *filename, int width, int height, unsigned char *rgbdata)
{
   warning("Would write image %s, but was not compiled "
           "with image support\n", filename);
   return -1;
}

#endif
