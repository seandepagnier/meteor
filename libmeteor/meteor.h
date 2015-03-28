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

#ifndef LIB_METEOR_H
#define LIB_METEOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* formats */
#define METEOR_INDEX      0
#define METEOR_COORDS     1
#define METEOR_NORMALS    2
#define METEOR_COLORS     4
#define METEOR_TEXCOORDS  8

/* types */
enum {METEOR_INT, METEOR_UNSIGNED_INT,
      METEOR_FLOAT, METEOR_DOUBLE, METEOR_LONG_DOUBLE};

void meteorSetSize(double xmin, double xmax, double ymin, double ymax,
                 double zmin, double zmax, double step);

void meteorReset(int format);

int meteorFormat(void);

int meteorBuild(void);

int meteorMerge(void);
int meteorAggregate(void);
void meteorClip(double (*func)(double, double, double));
void meteorCorrectTexCoords(void);

double meteorPropagate(int);

const char *meteorError(void);

/* high level meteor file io routines */
enum {METEOR_FILE_FORMAT_TEXT, METEOR_FILE_FORMAT_BINARY,
      METEOR_FILE_FORMAT_VIDEOSCAPE, METEOR_FILE_FORMAT_WAVEFRONT};

#ifdef _STDIO_H
int meteorLoad(FILE *file, int dataformat);
int meteorSave(FILE *file, int dataformat);
#endif

/* meteor status functions */
int meteorPointCount(void);
int meteorPointCreatedCount(void);
int meteorTriangleCount(void);
int meteorTriangleCreatedCount(void);
int meteorTriangleMergeableCount(void);

/*  meteor data transfer routines */
void meteorRewind(void);

int meteorReadPoints(int count, int format, int type, void *data);
int meteorReadTriangles(int count, int format, int type, void *data);

int meteorWritePoints(int count, int format, int type, const void *data);
int meteorWriteTriangles(int count, int format, int type, const void *data);

/* transformations */
void meteorMultMatrix(double m[16]);
void meteorRotate(double angle, double x, double y, double z);
void meteorTranslate(double x, double y, double z);
void meteorScale(double x, double y, double z);

/* meteor data generation callbacks */
void meteorFunc(double (*func)(double x, double y, double z));
void meteorNormalFunc(void (*func)(double[3], double[3]));
void meteorColorFunc(void (*func)(double[3], double[3]));
void meteorTexCoordFunc(void (*func)(double[3], double[3]));

#ifdef __cplusplus
}
#endif

#endif
