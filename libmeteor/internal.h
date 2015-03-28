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

#include "config.h"

#pragma GCC visibility push(hidden)

/* set up type */
#if defined(USE_FLOAT_FORMAT)
typedef float mfloat;
#define sqrt sqrtf
#elif defined(USE_DOUBLE_FORMAT)
typedef double mfloat;
#elif defined(USE_LONG_DOUBLE_FORMAT)
typedef long double mfloat;
#define sqrt sqrtl
#else
#error "no type selected"
#endif

struct tri_t;

struct trilist_t {
   struct tri_t *tri;
   struct trilist_t *next;
};

struct point_t {
   mfloat pos[3];

   /* the Q matrix is used for quadric merging, the kd members
      are used in the kd tree which does not use the cost matrix */
   union {
      mfloat Q[10]; /* cost matrix */ 
      struct {
         struct point_t *kdl, *kdr, **kdp;
         int kdaxis; /* depth in kd-tree */
      };
   };
 
   struct trilist_t *tris;

   union {
      mfloat cost; /* cost of making the merge */
      mfloat cut; /* used for cutting */
   };

   int index; /* index back into heap */
   union {
      struct point_t *mp; /* least cost point to merge to */
      struct point_t *next; /* when in a linked list */
   };

   /* extra data (normal, texture, color) */
   mfloat data[];
};

struct tri_t {
   struct tri_t *prev, *next;
   struct point_t *p[3]; /* our points */
};

/* meteor routines */
void CalculateHeap(int start, int end);
void addToTriList(struct point_t *p, struct tri_t *q);

extern void (*CalculateOptimalPoint)(struct point_t *p, int init);

extern unsigned int CreatedPoints, FreedPoints;
extern unsigned int CreatedTriangles, FreedTriangles;
extern unsigned int SortedTriangleCount;

#define PointCount (CreatedPoints - FreedPoints)
#define TriangleCount (CreatedTriangles - FreedTriangles)

extern int DataFormat;
extern int NormalOffset, ColorOffset, TexCoordOffset;

/* a means to fatally abort */
#define die(...) (fflush(stdout), fprintf(stderr, "[libmeteor] "__VA_ARGS__), abort())

/* mem */
extern int DataParts;

extern struct tri_t *Tris;

struct point_t *AllocPoint(void);
void FreeTriList(struct point_t *p);
void FreeTriListItem(struct trilist_t **l);
void FreeTri(struct tri_t *t);
struct trilist_t *AllocTriList(void);
struct tri_t *AllocTri(void);

void freeTris(void);
void freeMem(void);

void relinquishMem(void);

/* data */
extern int MeshModified;

/* heap */
enum {HEAP_NONE, HEAP_MIN, HEAP_AGGREGATE};
extern struct point_t **Heap;
extern int heapSize, heapMode;

void heapInsert(struct point_t *p);
void heapInsertUnsorted(struct point_t *p);
void heapRemove(struct point_t *p);
void heapUpdate(struct point_t *p);
void heapClear(void);
void heapSetSize(int s);

/* kd tree for aggregation */
void kdTreeInsert(struct point_t *p);
void kdTreeRemove(struct point_t *p);
void kdTreeUpdate(struct point_t *p);
void kdTreeClear(void);

/* building */
void AddQTri(struct tri_t *tri);
void buildQHeap(void);
void NewTriangle(struct point_t *p1, struct point_t *p2, struct point_t *p3);
struct point_t *NewPoint(void);

double (*Func)(double, double, double);
void (*NormalFunc)(double[3], double[3]);
void (*ColorFunc)(double[3], double[3]);
void (*TexCoordFunc)(double[3], double[3]);

#pragma GCC visibility pop
