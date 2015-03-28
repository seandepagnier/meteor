/*
 * Copyright (C) 2007  Sean D'Epagnier   All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* this file contains helper functions for saving and loading meteor data.
   without internal.h it is stand alone from the rest of the library. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <math.h>

#include "meteor.h"

#define TEXT_PRECISION "%.7g"
#define TEXT_PRECISION2 TEXT_PRECISION" "TEXT_PRECISION
#define TEXT_PRECISION3 TEXT_PRECISION2" "TEXT_PRECISION

extern int newmeteorerror;
extern char meteorerror[256];

#define ERROR(...) do { sprintf(meteorerror, __VA_ARGS__); \
                        if(line) { char b2[256]; strcpy(b2, meteorerror); \
                                   sprintf(meteorerror, "line %d: %s", line, b2); \
                                   newmeteorerror = 1; } \
                        return -1; } while(0)

#define TRY(x) do { if(x) { if(errno) strcpy(meteorerror, strerror(errno)); \
                                      newmeteorerror = 1; return -1; } } while(0)

static unsigned long tobyte(double val)
{
   if(val >= 1.0)
      return 0xff;
   if(val <= 0.0)
      return 0x00;
   return val * 255.0;
}

int meteorSave(FILE *file, int fileformat)
{
   int line = 0;
   if(!file) {
      switch(fileformat) {
      case METEOR_FILE_FORMAT_TEXT:      case METEOR_FILE_FORMAT_BINARY:
      case METEOR_FILE_FORMAT_WAVEFRONT: case METEOR_FILE_FORMAT_VIDEOSCAPE:
         return 0;
      }
      ERROR("Format not available");
   }

   int format = meteorFormat();
   int points = meteorPointCount(), triangles = meteorTriangleCount();
   switch(fileformat) {
   case METEOR_FILE_FORMAT_TEXT:
      TRY(fprintf(file, "%d %d %d\n", format, points, triangles) < 1);
      line = 1;
      break;
   case METEOR_FILE_FORMAT_BINARY:
      TRY((fwrite(&format, sizeof format, 1, file)
           + fwrite(&points, sizeof format, 1, file)
           + fwrite(&triangles, sizeof format, 1, file)) != 3);
      break;
   case METEOR_FILE_FORMAT_WAVEFRONT:
      /* no header */
      break;
   case METEOR_FILE_FORMAT_VIDEOSCAPE:
      TRY(!fputs("3DG1\n", file));
      TRY(fprintf(file, "%d\n", points) == 0);
      break;
   default:
      ERROR("Format not available");
   }

   meteorRewind();
   
   int dataparts = !!(format&METEOR_NORMALS) + !!(format&METEOR_COLORS)
      + !!(format&METEOR_TEXCOORDS);

   double data[12];
   int inds[3], i, j;

   switch(fileformat) {
   case METEOR_FILE_FORMAT_TEXT:
      for(i=0; i<points; i++) {
         TRY(meteorReadPoints(1, format, METEOR_DOUBLE, data) != 1);
         int c = fprintf(file, TEXT_PRECISION3, data[0], data[1], data[2]);
         for(j = 3; j <= 3*dataparts; j+=3)
            c += fprintf(file, " "TEXT_PRECISION3, data[j+0], data[j+1], data[j+2]);
         TRY(c <= 0 || fputc('\n', file) == EOF);
      }
      for(i = 0; i<triangles; i++) {
         TRY(meteorReadTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
         TRY(fprintf(file, "%d %d %d\n", inds[0], inds[1], inds[2]) < 0);
      }
      break;
   case METEOR_FILE_FORMAT_BINARY:
      for(i=0; i<points; i++) {
         TRY(meteorReadPoints(1, format, METEOR_DOUBLE, data) != 1);
         TRY(fwrite(data, 3 * (sizeof *data) * (dataparts + 1), 1, file) != 1);
      }
      for(i = 0; i<triangles; i++) {
         TRY(meteorReadTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
         TRY(fwrite(inds, 3 * (sizeof *inds), 1, file) != 1);
      }
      break;
   case METEOR_FILE_FORMAT_WAVEFRONT:
      for(i=0; i<points; i++) {
         TRY(meteorReadPoints(1, METEOR_COORDS, METEOR_DOUBLE, data) != 1);
         TRY(fprintf(file, "v "TEXT_PRECISION3"\n",
                     data[0], data[1], data[2]) < 1);
      }

      meteorRewind();

      if(format & METEOR_NORMALS)
         for(i=0; i<points; i++) {
            TRY(meteorReadPoints(1, METEOR_NORMALS, METEOR_DOUBLE, data) != 1);
            TRY(fprintf(file, "vn "TEXT_PRECISION3"\n",
                        data[0], data[1], data[2]) < 1);
         }

      meteorRewind();

      if(format & METEOR_TEXCOORDS)
         for(i=0; i<points; i++) {
            TRY(meteorReadPoints(1, METEOR_TEXCOORDS, METEOR_DOUBLE, data) != 1);
            if(data[2])
               TRY(fprintf(file, "vt "TEXT_PRECISION3"\n",
                           data[0], data[1], data[2]) < 1);
            else
               TRY(fprintf(file, "vt "TEXT_PRECISION2"\n",
                        data[0], data[1]) < 1);
         }

      for(i = 0; i<triangles; i++) {
         TRY(meteorReadTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
         TRY(fprintf(file, "f") < 1);
         for(j = 0; j<3; j++) {
            char num[20];
            sprintf(num, "%d", inds[j] + 1);
            if(format & METEOR_NORMALS)
               TRY(fprintf(file, " %s/%s/%s", num,
                           format & METEOR_TEXCOORDS ? num : "", num) < 1);
            else
               TRY(fprintf(file, " %s%s%s", num,
                           format & METEOR_TEXCOORDS ? "/" : "",
                           format & METEOR_TEXCOORDS ? num : "") < 1);
         }
         TRY(fprintf(file, "\n") < 1);
      }
      break;
   case METEOR_FILE_FORMAT_VIDEOSCAPE:
      for(i=0; i<points; i++) {
         TRY(meteorReadPoints(1, format, METEOR_DOUBLE, data) != 1);
         TRY(fprintf(file, TEXT_PRECISION3"\n", data[0], data[1], data[2]) < 1);
      }
      
      unsigned long color = 0xffffff;
      for(i = 0; i<triangles; i++) {            
         TRY(meteorReadTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);

         if(format & METEOR_COLORS) {
            TRY(meteorReadTriangles(1, METEOR_COLORS, METEOR_DOUBLE, data) != 1);
            color = tobyte((data[0] + data[3] + data[6])/3) << 16
               | tobyte((data[1] + data[4] + data[7])/3) << 8
               | tobyte((data[2] + data[5] + data[8])/3) << 0;
         }
         TRY(fprintf(file, "3 %d %d %d 0x%x\n", inds[0], inds[1], inds[2], color) == 0);
      }
      break;
   }

   return 0;
}

int meteorLoad(FILE *file, int fileformat)
{
   int line = 0;
   if(!file) {
      switch(fileformat) {
      case METEOR_FILE_FORMAT_TEXT:       case METEOR_FILE_FORMAT_BINARY:
      case METEOR_FILE_FORMAT_VIDEOSCAPE:
         return 0;
      }
      ERROR("Format not available");
   }

   int i, j, format, points, triangles;

   switch(fileformat) {
   case METEOR_FILE_FORMAT_TEXT:
      line = 1;
      TRY(fscanf(file, "%d %d %d\n", &format, &points, &triangles) != 3);
      line++;
      break;
   case METEOR_FILE_FORMAT_BINARY:
      TRY(fread(&format, sizeof format, 1, file)
          + fread(&points, sizeof format, 1, file)
          + fread(&triangles, sizeof format, 1, file) != 3);
      if(format & ~(METEOR_COORDS | METEOR_NORMALS
                    | METEOR_COLORS | METEOR_TEXCOORDS))
         ERROR("Invalid format");
      break;
   case METEOR_FILE_FORMAT_VIDEOSCAPE:
      {
         char buffer[1024];
         line = 1;
         TRY(!fgets(buffer, sizeof buffer, file));
         if(strcmp(buffer, "3DG1\n"))
            ERROR("Unhandled magic number: %s", buffer);
         line++;
         format = METEOR_COORDS | METEOR_COLORS;
         TRY(fscanf(file, "%d\n", &points) != 1);
      } break;
   default:
      ERROR("Format not available");
   }
   
   meteorReset(format);

   int dataparts = !!(format&METEOR_NORMALS) + !!(format&METEOR_COLORS)
      + !!(format&METEOR_TEXCOORDS);

   double data[12];
   int inds[3];

   switch(fileformat) {
   case METEOR_FILE_FORMAT_TEXT:
      for(i=0; i<points; i++) {
         int c = fscanf(file, "%lf %lf %lf", data+0, data+1, data+2);
         int j;
         for(j = 3; j <= 3*dataparts; j+=3)
            c += fscanf(file, " %lf %lf %lf", data+j+0, data+j+1, data+j+2);
         TRY(c != 3*(dataparts + 1));
         TRY(meteorWritePoints(1, format, METEOR_DOUBLE, data) != 1);
         line++;
      }

      for(i = 0; i<triangles; i++) {
         TRY(fscanf(file, "%d %d %d\n", inds+0, inds+1, inds+2) != 3);
         TRY(meteorWriteTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
         line++;
      }
      break;
   case METEOR_FILE_FORMAT_BINARY:
      for(i=0; i<points; i++) {
         TRY(fread(data, 3 * (sizeof *data) * (dataparts + 1), 1, file) != 1);
         TRY(meteorWritePoints(1, format, METEOR_DOUBLE, data) != 1);
         line++;
      }

      for(i = 0; i<triangles; i++) {
         TRY(fread(inds, 3 * (sizeof *inds), 1, file) != 1);
         TRY(meteorWriteTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
         line++;
      }
      break;
   case METEOR_FILE_FORMAT_VIDEOSCAPE:
      line = 3;
      double (*avgcolor)[4] = malloc(points * (sizeof *avgcolor));
      for(i=0; i<points; i++) {
         TRY(fscanf(file, "%lf %lf %lf\n", data+0, data+1, data+2) != 3);
         TRY(meteorWritePoints(1, format, METEOR_DOUBLE, data) != 1);
         
         for(j = 0; j < 4; j++)
            avgcolor[i][j] = 0;
         line++;
      }

      char buffer[256];
      while(fgets(buffer, sizeof buffer, file)) {
         char *ptr = buffer, *endptr;
         int vertices = strtol(ptr, &endptr, 10);
         int indexes[6];

         if(ptr == endptr)
            ERROR("syntax error");
         ptr = endptr;

         if(vertices < 3 || vertices > 6)
            ERROR("Unsupported vertice count: %d", vertices);

         int i;
         for(i = 0; i<vertices; i++) {
            while(*ptr && !isdigit(*ptr)) ptr++;
            indexes[i] = strtol(ptr, &endptr, 10);
            if(ptr == endptr)
               ERROR("syntax error");
            ptr = endptr;
            if(i > 1) {
               int inds[3] = {indexes[0], indexes[i-1], indexes[i]};
               TRY(meteorWriteTriangles(1, METEOR_INDEX, METEOR_INT, inds) != 1);
            }
         }

         while(*ptr && !isdigit(*ptr)) ptr++;
         unsigned long color = strtol(ptr, &endptr, 0);
         if(ptr == endptr)
            ERROR("syntax error");

         double c[4];
         c[0] = (double)((color&0xff0000) >> 16) / 255.0;
         c[1] = (double)((color&0x00ff00) >>  8) / 255.0;
         c[2] = (double)((color&0x0000ff) >>  0) / 255.0;
         c[3] = 1;

         for(i = 0; i<vertices; i++)
            for(j = 0; j<4; j++)
               avgcolor[indexes[i]][j] += c[j];

         line++;
      }

      meteorRewind();
      for(i=0; i<points; i++) {
         for(j = 0; j < 3; j++)
            data[j] = avgcolor[i][j] / avgcolor[i][3];
         TRY(meteorWritePoints(1, METEOR_COLORS, METEOR_DOUBLE, data) != 1);
      }
      free(avgcolor);
      break;
   }
   return 0;
}
