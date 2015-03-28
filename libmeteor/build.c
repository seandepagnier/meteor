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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "internal.h"

#include "meteor.h"
#include "linalg.h"

enum BuildStates {NOTSTARTED, NOSYNC, SYNC};
static int BuildState = NOTSTARTED;

/* these are the functions in use */
double (*Func)(double, double, double);
void (*NormalFunc)(double[3], double[3]);
void (*ColorFunc)(double[3], double[3]);
void (*TexCoordFunc)(double[3], double[3]);

/* stores the generated points (or null) for the slices of the meteor,
   A is the y-z plane, and B is just the y line in that plane */
static struct point_t **TetraPointsA[2];
static struct point_t **TetraPointsB[2];

static mfloat *TetraPointValsB[2];

static int TetraPointPageB; /* page in use */

static int UnsortedStart; /* index in heap of the first unsorted point */
static unsigned int SortedPointCount; /* only used internally */

struct point_t *NewPoint(void)
{
   struct point_t *p = AllocPoint();
   p->tris = NULL;
   heapInsertUnsorted(p);
   return p;
}

static struct tri_t *LastTri;

/* add pairs to  p1, p2, and p3 so that this triangle exists */
void NewTriangle(struct point_t *p1, struct point_t *p2, struct point_t *p3)
{
#ifdef DEBUG
   if(!p1 || !p2 || !p3)
      die("null points in triangle!\n");
#endif

   struct tri_t *Tri = AllocTri();

   Tri->p[0] = p1;
   Tri->p[1] = p2;
   Tri->p[2] = p3;

   if(heapMode == HEAP_MIN)
      if(LastTri == Tris)
         LastTri = Tri;

   addToTriList(p1, Tri);
   addToTriList(p2, Tri);
   addToTriList(p3, Tri);
}

void meteorFreeMem(void)
{
   /* free regions allocated for building */
   free(TetraPointsA[0]);
   free(TetraPointsA[1]);
   TetraPointsA[0] = TetraPointsA[1] = NULL;

   free(TetraPointsB[0]);
   free(TetraPointsB[1]);
   TetraPointsB[0] = TetraPointsB[1] = NULL;

   free(TetraPointValsB[0]);
   free(TetraPointValsB[1]);
   TetraPointValsB[0] = TetraPointValsB[1] = NULL;

   /* free points triangles and triangle lists */
   freeMem();
   relinquishMem();

   /* free heap */
   free(Heap);
   Heap = NULL;
}

/* create a new point to be used by tris in the meteor, the position
   should be interpolated between p1 and p2 (edge of tetrahedron) */
static inline struct point_t *MakePoint(mfloat p1[4], mfloat p2[4])
{
   /* if w1 and w2 have the same sign, they don't cut the surface */
   if(p1[3] * p2[3] >= 0)
      return NULL;

   struct point_t *p = NewPoint();

   if(heapMode == HEAP_MIN) {
      int i;
      for(i = 0; i<10; i++)
         p->Q[i] = 0;
   }

   mfloat q1[4] = {p1[0], p1[1], p1[2], p1[3]};
   mfloat q2[4] = {p2[0], p2[1], p2[2], p2[3]};

   iterativeimprove(p->pos, q1, q2, Func);

   /* calculate any additional data used by this point,
      this could be defered until later since it is possible
      to eliminate this point with merging before this data is ever used */
#ifdef USE_DOUBLE_FORMAT
   if(DataFormat & METEOR_NORMALS)
      NormalFunc(p->data + NormalOffset, p->pos);
   if(DataFormat & METEOR_COLORS)
      ColorFunc(p->data + ColorOffset, p->pos);
   if(DataFormat & METEOR_TEXCOORDS)
      TexCoordFunc(p->data + TexCoordOffset, p->pos);
#else
   double pos[3] = {p->pos[0], p->pos[1], p->pos[2]}, data[3];
#define SETDATA(x) (x)[0] = data[0], (x)[1] = data[1], (x)[2] = data[2]
   if(DataFormat & METEOR_NORMALS)
      NormalFunc(data, pos), SETDATA(p->data + NormalOffset);
   if(DataFormat & METEOR_COLORS)
      ColorFunc(data, pos), SETDATA(p->data + ColorOffset);
   if(DataFormat & METEOR_TEXCOORDS)
      TexCoordFunc(data, pos), SETDATA(p->data + TexCoordOffset);
#endif
   return p;
}

/* take the 6 sides of a tetrahedron and also it's four points and
   add the appropriate triangles in the right orientation */
static inline void addTetra(struct point_t *p1, struct point_t *p2,
                            struct point_t *p3, struct point_t *p4,
                            struct point_t *p5, struct point_t *p6,
                            mfloat v1)
{
   /* this jump table is used to calculate all 7 possible interesting
      combinations for marching tetrahedrons given points along
      the 6 edges of a tetrahedron (or null if the surface did not cut this edge) */
   static const int jmptable[8] = {1, 5, 6, 4, 7, 2, 3, 0};

#define T(a, b, c) if(v1<0) NewTriangle(p##a, p##b, p##c); else NewTriangle(p##a, p##c, p##b);
   switch(jmptable[(!p1*2+!p2)*2+!p3]) {
   case 1: T(1, 2, 3) break;
   case 2: T(2, 5, 4) break;
   case 3: T(3, 6, 5) break;
   case 4: T(1, 4, 6) break;
   case 5: T(1, 2, 6); T(2, 5, 6); break;
   case 6: T(1, 4, 3); T(3, 4, 5); break;
   case 7: T(2, 3, 4); T(3, 6, 4); break;
   }
#undef T
}

/* uh.. the hideous expanded version of the above.. about 2x faster,
   see tetracalc.c for the code that generated this code */
static inline void addCubeTetras(struct point_t **p, mfloat pv)
{
#define T(a, b, c) if(pv<0) NewTriangle(p[a], p[b], p[c]); else NewTriangle(p[a], p[c], p[b]);
   switch((((((!p[4]*2+!p[2])*2+!p[1])*2+!p[8])*2+!p[14])*2+!p[15])*2+!p[18]) {
   case 0: T(18, 15, 4) T(18, 4, 2) T(18, 2, 1) T(18, 1, 8) T(18, 8, 14) T(18, 14, 15) break;
   case 1: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7)
              T(1, 8, 6) T(8, 9, 6) T(8, 14, 9) T(14, 11, 9) T(14, 15, 11) T(15, 12, 11) break;
   case 2: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 1) T(18, 1, 8) T(18, 8, 14)
        T(18, 14, 12) T(14, 13, 12) break;
   case 3: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6) T(8, 9, 6)
              T(8, 14, 9) T(14, 11, 9) T(14, 13, 11) break;
   case 4: T(18, 15, 4) T(18, 4, 2) T(18, 2, 1) T(18, 1, 8) T(18, 8, 11) T(8, 10, 11)
              T(18, 11, 15) T(15, 11, 13) break;
   case 5: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7)
              T(1, 8, 6) T(8, 9, 6) T(8, 10, 9) T(15, 12, 13) break;
   case 6: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 1) T(18, 1, 8) T(18, 8, 11)
              T(8, 10, 11) T(18, 11, 12) break;
   case 7: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6)
              T(8, 9, 6) T(8, 10, 9) break;
   case 8: T(18, 15, 4) T(18, 4, 2) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9) T(18, 9, 14)
              T(14, 9, 10) T(18, 14, 15) break;
   case 9: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7)
              T(1, 5, 6) T(14, 11, 10) T(14, 15, 11) T(15, 12, 11) break;
   case 10: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9)
               T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 11: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6)
               T(14, 11, 10) T(14, 13, 11) break;
   case 12: T(18, 15, 4) T(18, 4, 2) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9) T(18, 9, 11)
               T(18, 11, 15) T(15, 11, 13) break;
   case 13: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7)
               T(1, 5, 6) T(15, 12, 13) break;
   case 14: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9)
               T(18, 9, 11) T(18, 11, 12) break;
   case 15: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6) break;
   case 16: T(18, 15, 4) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8) T(8, 6, 5)
               T(18, 8, 14) T(18, 14, 15) break;
   case 17: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(8, 9, 5)
               T(8, 14, 9) T(14, 11, 9) T(14, 15, 11) T(15, 12, 11) break;
   case 18: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 14) T(18, 14, 12) T(14, 13, 12) break;
   case 19: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(8, 9, 5) T(8, 14, 9)
               T(14, 11, 9) T(14, 13, 11) break;
   case 20: T(18, 15, 4) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8) T(8, 6, 5)
               T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 21: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(8, 9, 5)
               T(8, 10, 9) T(15, 12, 13) break;
   case 22: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 11) T(8, 10, 11) T(18, 11, 12) break;
   case 23: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(8, 9, 5) T(8, 10, 9) break;
   case 24: T(18, 15, 4) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9) T(18, 9, 14)
               T(14, 9, 10) T(18, 14, 15) break;
   case 25: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(14, 11, 10)
               T(14, 15, 11) T(15, 12, 11) break;
   case 26: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9)
               T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 27: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(14, 11, 10) T(14, 13, 11) break;
   case 28: T(18, 15, 4) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9) T(18, 9, 11)
               T(18, 11, 15) T(15, 11, 13) break;
   case 29: T(15, 4, 12) T(4, 17, 12) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) T(15, 12, 13) break;
   case 30: T(18, 12, 4) T(4, 12, 16) T(18, 4, 2) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9)
               T(18, 9, 11) T(18, 11, 12) break;
   case 31: T(4, 17, 16) T(4, 2, 17) T(2, 7, 17) T(2, 0, 7) break;
   case 32: T(18, 15, 4) T(18, 4, 7)
               T(4, 3, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 8) T(18, 8, 14) T(18, 14, 15) break;
   case 33: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6)
               T(8, 14, 9) T(14, 11, 9) T(14, 15, 11) T(15, 12, 11) break;
   case 34: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0)
               T(18, 1, 8) T(18, 8, 14) T(18, 14, 12) T(14, 13, 12) break;
   case 35: T(4, 17, 16) T(4, 3, 17) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 14, 9)
               T(14, 11, 9) T(14, 13, 11) break;
   case 36: T(18, 15, 4) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 8)
               T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 37: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6)
               T(8, 10, 9) T(15, 12, 13) break;
   case 38: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0)
               T(18, 1, 8) T(18, 8, 11) T(8, 10, 11) T(18, 11, 12) break;
   case 39: T(4, 17, 16) T(4, 3, 17) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 10, 9) break;
   case 40: T(18, 15, 4) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 9)
               T(1, 5, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 41: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(1, 6, 0) T(1, 5, 6) T(14, 11, 10)
               T(14, 15, 11) T(15, 12, 11) break;
   case 42: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0)
               T(18, 1, 9) T(1, 5, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 43: T(4, 17, 16) T(4, 3, 17) T(1, 6, 0) T(1, 5, 6) T(14, 11, 10) T(14, 13, 11) break;
   case 44: T(18, 15, 4) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 9)
               T(1, 5, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 45: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(1, 6, 0) T(1, 5, 6) T(15, 12, 13) break;
   case 46: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 1) T(1, 7, 0)
               T(18, 1, 9) T(1, 5, 9) T(18, 9, 11) T(18, 11, 12) break;
   case 47: T(4, 17, 16) T(4, 3, 17) T(1, 6, 0) T(1, 5, 6) break;
   case 48: T(18, 15, 4) T(18, 4, 7)
               T(4, 3, 7) T(18, 7, 6) T(18, 6, 8) T(8, 6, 5) T(18, 8, 14) T(18, 14, 15) break;
   case 49: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(8, 9, 5) T(8, 14, 9) T(14, 11, 9)
               T(14, 15, 11) T(15, 12, 11) break;
   case 50: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 14) T(18, 14, 12) T(14, 13, 12) break;
   case 51: T(4, 17, 16) T(4, 3, 17) T(8, 9, 5) T(8, 14, 9) T(14, 11, 9) T(14, 13, 11) break;
   case 52: T(18, 15, 4) T(18, 4, 7) T(4, 3, 7) T(18, 7, 6) T(18, 6, 8) T(8, 6, 5)
               T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 53: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(8, 9, 5) T(8, 10, 9) T(15, 12, 13) break;
   case 54: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 11) T(8, 10, 11) T(18, 11, 12) break;
   case 55: T(4, 17, 16) T(4, 3, 17) T(8, 9, 5) T(8, 10, 9) break;
   case 56: T(18, 15, 4) T(18, 4, 7)
               T(4, 3, 7) T(18, 7, 6) T(18, 6, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 57: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(14, 11, 10) T(14, 15, 11) T(15, 12, 11) break;
   case 58: T(18, 12, 4) T(4, 12, 16) T(18, 4, 7) T(4, 3, 7) T(18, 7, 6) T(18, 6, 9)
               T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 59: T(4, 17, 16) T(4, 3, 17) T(14, 11, 10) T(14, 13, 11) break;
   case 60: T(18, 15, 4) T(18, 4, 7)
               T(4, 3, 7) T(18, 7, 6) T(18, 6, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 61: T(15, 4, 12) T(4, 17, 12) T(4, 3, 17) T(15, 12, 13) break;
   case 62: T(18, 12, 4) T(4, 12, 16)
               T(18, 4, 7) T(4, 3, 7) T(18, 7, 6) T(18, 6, 9) T(18, 9, 11) T(18, 11, 12) break;
   case 63: T(4, 17, 16) T(4, 3, 17) break;
   case 64: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3)
               T(18, 2, 1) T(18, 1, 8) T(18, 8, 14) T(18, 14, 15) break;
   case 65: T(15, 16, 12) T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6) T(8, 9, 6)
               T(8, 14, 9) T(14, 11, 9) T(14, 15, 11) T(15, 12, 11) break;
   case 66: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 8) T(18, 8, 14)
               T(18, 14, 12) T(14, 13, 12) break;
   case 67: T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6) T(8, 9, 6) T(8, 14, 9)
               T(14, 11, 9) T(14, 13, 11) break;
   case 68: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 8)
               T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 69: T(15, 16, 12) T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6) T(8, 9, 6)
               T(8, 10, 9) T(15, 12, 13) break;
   case 70: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 8) T(18, 8, 11)
               T(8, 10, 11) T(18, 11, 12) break;
   case 71: T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 8, 6) T(8, 9, 6) T(8, 10, 9) break;
   case 72: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 9)
               T(1, 5, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 73: T(15, 16, 12) T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6) T(14, 11, 10)
               T(14, 15, 11) T(15, 12, 11) break;
   case 74: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9)
               T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 75: T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6) T(14, 11, 10) T(14, 13, 11) break;
   case 76: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 9)
               T(1, 5, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 77: T(15, 16, 12) T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6) T(15, 12, 13) break;
   case 78: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 1) T(18, 1, 9) T(1, 5, 9)
               T(18, 9, 11) T(18, 11, 12) break;
   case 79: T(2, 7, 3) T(2, 1, 7) T(1, 6, 7) T(1, 5, 6) break;
   case 80: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6)
               T(18, 6, 8) T(8, 6, 5) T(18, 8, 14) T(18, 14, 15) break;
   case 81: T(15, 16, 12) T(2, 7, 3) T(2, 0, 7) T(8, 9, 5) T(8, 14, 9) T(14, 11, 9)
               T(14, 15, 11) T(15, 12, 11) break;
   case 82: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 14) T(18, 14, 12) T(14, 13, 12) break;
   case 83: T(2, 7, 3) T(2, 0, 7) T(8, 9, 5) T(8, 14, 9) T(14, 11, 9) T(14, 13, 11) break;
   case 84: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6)
               T(18, 6, 8) T(8, 6, 5) T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 85: T(15, 16, 12) T(2, 7, 3) T(2, 0, 7) T(8, 9, 5) T(8, 10, 9) T(15, 12, 13) break;
   case 86: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6) T(18, 6, 8)
               T(8, 6, 5) T(18, 8, 11) T(8, 10, 11) T(18, 11, 12) break;
   case 87: T(2, 7, 3) T(2, 0, 7) T(8, 9, 5) T(8, 10, 9) break;
   case 88: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6)
               T(18, 6, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 89: T(15, 16, 12) T(2, 7, 3) T(2, 0, 7) T(14, 11, 10) T(14, 15, 11) T(15, 12, 11) break;
   case 90: T(18, 12, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9)
               T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 91: T(2, 7, 3) T(2, 0, 7) T(14, 11, 10) T(14, 13, 11) break;
   case 92: T(18, 15, 17) T(15, 16, 17) T(18, 17, 2) T(2, 17, 3) T(18, 2, 6) T(2, 0, 6)
               T(18, 6, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 93: T(15, 16, 12) T(2, 7, 3) T(2, 0, 7) T(15, 12, 13) break;
   case 94: T(18, 12, 17) T(18, 17, 2)
               T(2, 17, 3) T(18, 2, 6) T(2, 0, 6) T(18, 6, 9) T(18, 9, 11) T(18, 11, 12) break;
   case 95: T(2, 7, 3) T(2, 0, 7) break;
   case 96: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 1)
               T(1, 7, 0) T(18, 1, 8) T(18, 8, 14) T(18, 14, 15) break;
   case 97: T(15, 16, 12) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 14, 9) T(14, 11, 9)
               T(14, 15, 11) T(15, 12, 11) break;
   case 98: T(18, 12, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 8) T(18, 8, 14)
               T(18, 14, 12) T(14, 13, 12) break;
   case 99: T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 14, 9) T(14, 11, 9) T(14, 13, 11) break;
   case 100: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 8)
                T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 101: T(15, 16, 12) T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 10, 9) T(15, 12, 13) break;
   case 102: T(18, 12, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 8) T(18, 8, 11)
                T(8, 10, 11) T(18, 11, 12) break;
   case 103: T(1, 6, 0) T(1, 8, 6) T(8, 9, 6) T(8, 10, 9) break;
   case 104: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 9)
                T(1, 5, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 105: T(15, 16, 12) T(1, 6, 0) T(1, 5, 6) T(14, 11, 10) T(14, 15, 11) T(15, 12, 11) break;
   case 106: T(18, 12, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 9) T(1, 5, 9)
                T(18, 9, 14) T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 107: T(1, 6, 0) T(1, 5, 6) T(14, 11, 10) T(14, 13, 11) break;
   case 108: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 1) T(1, 7, 0) T(18, 1, 9)
                T(1, 5, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 109: T(15, 16, 12) T(1, 6, 0) T(1, 5, 6) T(15, 12, 13) break;
   case 110: T(18, 12, 17) T(18, 17, 7)
                T(18, 7, 1) T(1, 7, 0) T(18, 1, 9) T(1, 5, 9) T(18, 9, 11) T(18, 11, 12) break;
   case 111: T(1, 6, 0) T(1, 5, 6) break;
   case 112: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 6)
                T(18, 6, 8) T(8, 6, 5) T(18, 8, 14) T(18, 14, 15) break;
   case 113: T(15, 16, 12) T(8, 9, 5) T(8, 14, 9) T(14, 11, 9) T(14, 15, 11) T(15, 12, 11) break;
   case 114: T(18, 12, 17) T(18, 17, 7) T(18, 7, 6) T(18, 6, 8) T(8, 6, 5) T(18, 8, 14)
                T(18, 14, 12) T(14, 13, 12) break;
   case 115: T(8, 9, 5) T(8, 14, 9) T(14, 11, 9) T(14, 13, 11) break;
   case 116: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 6) T(18, 6, 8) T(8, 6, 5)
                T(18, 8, 11) T(8, 10, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 117: T(15, 16, 12) T(8, 9, 5) T(8, 10, 9) T(15, 12, 13) break;
   case 118: T(18, 12, 17) T(18, 17, 7)
                T(18, 7, 6) T(18, 6, 8) T(8, 6, 5) T(18, 8, 11) T(8, 10, 11) T(18, 11, 12) break;
   case 119: T(8, 9, 5) T(8, 10, 9) break;
   case 120: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 6)
                T(18, 6, 9) T(18, 9, 14) T(14, 9, 10) T(18, 14, 15) break;
   case 121: T(15, 16, 12) T(14, 11, 10) T(14, 15, 11) T(15, 12, 11) break;
   case 122: T(18, 12, 17) T(18, 17, 7) T(18, 7, 6) T(18, 6, 9) T(18, 9, 14)
                T(14, 9, 10) T(18, 14, 12) T(14, 13, 12) break;
   case 123: T(14, 11, 10) T(14, 13, 11) break;
   case 124: T(18, 15, 17) T(15, 16, 17) T(18, 17, 7) T(18, 7, 6)
                T(18, 6, 9) T(18, 9, 11) T(18, 11, 15) T(15, 11, 13) break;
   case 125: T(15, 16, 12) T(15, 12, 13) break;
   case 126: T(18, 12, 17) T(18, 17, 7) T(18, 7, 6) T(18, 6, 9)
                T(18, 9, 11) T(18, 11, 12) break;
   }
#undef T
}

static int xnum, ynum, znum, numA, numB;
static mfloat xmin, ymin, zmin, step;

/* there are two pages of tetrapoints that fill a y-z plane that
   are alternated, this way the points on the surface can be connected
   to the points below, this function fills a page with the values */

static void fillpage(mfloat x)
{
   mfloat *page = TetraPointValsB[TetraPointPageB];
   mfloat y, z;
   int yi, zi;
   for(y = ymin, yi = 0; yi < ynum - 1; y += step, yi++) {
      mfloat zf1 = 0;
      for(z = zmin + zf1, zi = 0; zi < znum - 1; z += step, zi++) {
	 mfloat d = Func(x, y, z);

	 /* distorts figure slightly but gets rid of 0 holes */
         const mfloat c = .0001;
	 if(fabs(d) < c)
            d = c;
	 *page++ = d;
      }
   }

   TetraPointPageB = !TetraPointPageB;
}

/* this is the main marching tetrahedrons algorithm, it runs
   through the y-z plane at x, and generates tetrahedrons that fill
   the space between this plane, and the plane above. */
static void tetrapage(mfloat x)
{
   struct point_t **ppB = TetraPointsB[TetraPointPageB];
   struct point_t **opB = TetraPointsB[!TetraPointPageB];
   
   mfloat x1 = x - step, x2 = x;
   mfloat y1, y2, z1, z2;
   int yi, zi;
   int i1 = 0, i2 = znum - 1, i3 = i2 + numA;

   mfloat *page1 = TetraPointValsB[TetraPointPageB];
   mfloat *page2 = TetraPointValsB[!TetraPointPageB];
   int pageA = 0;
   for(y1 = ymin, y2 = ymin+step, yi = 1; yi < ynum-1; y1 += step, y2+=step, yi++) {
      struct point_t **ppA = TetraPointsA[pageA];
      struct point_t **opA = TetraPointsA[!pageA];
      int j = 0;
      struct point_t *lastpoint = NULL, *curpoint;
      mfloat zf1 = 0, zf2 = 0;
      for(z1 = zmin, z2 = zmin+step, zi = 1; zi < znum-1; z1+=step, z2+=step, zi++) {
         mfloat p[8][4] = {{x1, y1, z1+zf1, page1[0]},
                           {x1, y1, z2+zf1, page1[1]},
                           {x1, y2, z1+zf2, page1[znum-1]},
                           {x1, y2, z2+zf2, page1[znum]},
                           {x2, y1, z1+zf1, page2[0]},
                           {x2, y1, z2+zf1, page2[1]},
                           {x2, y2, z1+zf2, page2[znum-1]},
                           {x2, y2, z2+zf2, page2[znum]}};
         
         struct point_t *cp[19] = {ppB[i1], ppB[i2], ppB[i2+1], ppB[i2+2],
                                     ppB[i3], ppA[j], ppA[j+1], ppA[j+2],
                                     lastpoint, opB[i1], opB[i2]};

         cp[14] = opA[j];

         /* calculate the 7 unknowns */
         opB[i2+1] = cp[11] = MakePoint(p[5], p[6]);
         opB[i2+2] = cp[12] = MakePoint(p[5], p[7]);
         opB[i3] = cp[13] = MakePoint(p[6], p[7]);
         opA[j+1] = cp[15] = MakePoint(p[7], p[2]);
         opA[j+2] = cp[16] = MakePoint(p[7], p[3]);
         curpoint = cp[17] = MakePoint(p[5], p[3]);
         struct point_t *c = cp[18] = MakePoint(p[2], p[5]);

         /* every cube has 6 tetrahedrons */
         if(x1 != xmin && y1 != ymin && z1 != zmin) {
#if 0 /* either way does the same thing, addCubeTetras is faster */
            addTetra(cp[0], cp[6], cp[1], cp[7], c, cp[2], p[0][3]);
            addTetra(cp[1], cp[6], cp[5], c, cp[9], cp[8], p[0][3]);
            addTetra(cp[3], cp[4], cp[17], cp[2], c, cp[7], p[3][3]);
            addTetra(cp[4], cp[16], cp[17], cp[15], cp[12], c, p[3][3]);
            addTetra(cp[10], cp[11], cp[14], cp[9], c, cp[8], p[6][3]);
            addTetra(cp[11], cp[13], cp[14], cp[12], cp[15], c, p[6][3]);
#else
            addCubeTetras(cp, p[2][3]);
#endif
         }

         /* update our indexes */
         page1++;
         page2++;
         i1++;
         i2+=2;
         i3++;
         j+=2;
         
         lastpoint = curpoint;
      }
      
      /* outer index update */
      page1++;
      page2++;
      
      i1 += numA + 1;
      i2 += znum + 2;
      i3 += numA + 1;
      
      pageA = !pageA;
   }
}

void meteorSetSize(double xmin1, double xmax1, double ymin1, double ymax1,
                 double zmin1, double zmax1, double step1)
{
   step = step1;

   xnum = (xmax1 - xmin1) / step1 + 1;
   ynum = (ymax1 - ymin1) / step1 + 1;
   znum = (zmax1 - zmin1) / step1 + 1;

   xmin = xmin1;
   ymin = ymin1;
   zmin = zmin1;

   numA = 2 * znum - 1;
   numB = ynum * (znum + numA - 1) - znum + 1;

   /* assuming memset 0 can be used to make all null pointers */
   TetraPointsA[0] = realloc(TetraPointsA[0], sizeof(*TetraPointsA[0]) * numA);
   memset(TetraPointsA[0], 0, sizeof(*TetraPointsA[0]) * numA);
   TetraPointsA[1] = realloc(TetraPointsA[1], sizeof(*TetraPointsA[1]) * numA);
   memset(TetraPointsA[1], 0, sizeof(*TetraPointsA[1]) * numA);

   TetraPointValsB[0] = realloc(TetraPointValsB[0],
                                sizeof(*TetraPointValsB[0]) * ynum * znum);
   TetraPointValsB[1] = realloc(TetraPointValsB[1],
                                sizeof(*TetraPointValsB[1]) * ynum * znum);
   TetraPointPageB = 0;

   TetraPointsB[0] = realloc(TetraPointsB[0], sizeof(*TetraPointsB[0]) * numB);
   memset(TetraPointsB[0], 0, sizeof(*TetraPointsB[0]) * numB);
   TetraPointsB[1] = realloc(TetraPointsB[1], sizeof(*TetraPointsB[1]) * numB);
   memset(TetraPointsB[1], 0, sizeof(*TetraPointsB[1]) * numB);
}

/* update points Q matrix to contain triangle offsets */
void AddQTri(struct tri_t *tri)
{
   struct point_t *p1 = tri->p[0], *p2 = tri->p[1], *p3 = tri->p[2];
   mfloat n[4], v[2][3], q[10];
   sub3(v[0], p2->pos, p1->pos);
   sub3(v[1], p3->pos, p1->pos);
   cross(n, v[0], v[1]);
      
   normalize(n);  /* changes results a bit if we don't do it */
   
   n[3] = -dot(n, p1->pos);
   
   q[0]  = n[0]*n[0], q[1]  = n[0]*n[1], q[2] = n[0]*n[2], q[3] = n[0]*n[3];
                      q[4]  = n[1]*n[1], q[5] = n[1]*n[2], q[6] = n[1]*n[3];
                                         q[7] = n[2]*n[2], q[8] = n[2]*n[3];
                                                           q[9] = n[3]*n[3];

   add4x4tri(p1->Q, q);
   add4x4tri(p2->Q, q);
   add4x4tri(p3->Q, q);
}

/* take all unsorted points and put them in a heap */
void buildQHeap(void)
{
   int i, j;
   if(heapMode != HEAP_MIN) {
      /* was not in heap mode, calculate Q matrix for existing points
         this gets calculated as future points are created */
      for(i = 0; i<PointCount; i++)
         for(j = 0; j<10; j++)
            Heap[i]->Q[j] = 0;

      struct tri_t *tri;
      for(tri = Tris->next; tri != Tris; tri = tri->next)
         AddQTri(tri);

      heapMode = HEAP_MIN;

      if(BuildState == NOTSTARTED) {
         SortedPointCount = PointCount;
         SortedTriangleCount = TriangleCount;
      }
   } else
      if(BuildState == NOTSTARTED)
         return;

   /* the heap is already in sync */
   if(BuildState == SYNC)
      return;

   int UnsortedStop = SortedPointCount - heapSize + UnsortedStart;
   while(UnsortedStart < UnsortedStop) {
      struct point_t *p = Heap[UnsortedStart];
      if(p->tris) {
         CalculateOptimalPoint(p, 1);
         if(BuildState == NOSYNC) /* discourage this merge */
            p->cost++;
         heapInsert(p);
      } else
         FreePoint(p);
      UnsortedStart++;
   }

   BuildState = SYNC;
}

static void UnsortedRemoveNoTris(void)
{
   /* get rid of any points without triangles, and stuff rest in heap */
   int i;
   for(i = 0; i<PointCount; i++) {
      struct point_t *p = Heap[i];
      if(!p->tris) {
         FreePoint(p);
         Heap[i] = Heap[PointCount];
         Heap[i]->index = i;
      }
   }
}

int meteorBuild(void)
{
   static int xi;
   static mfloat x;

   if(BuildState == NOTSTARTED) {
      /* just started building */
      freeMem();
      TetraPointPageB = 0;
      fillpage(xmin);

      xi = 0;
      UnsortedStart = 0;
      x = xmin + step;

      LastTri = Tris;
   }

   SortedPointCount = PointCount;
   SortedTriangleCount = TriangleCount;

   BuildState = NOSYNC;
   
   /* compact heap */
   if(heapSize != UnsortedStart) {
      int i, j;
      int end = PointCount - heapSize + UnsortedStart;
      for(i = UnsortedStart, j = heapSize; i<end; i++, j++) {
         Heap[j] = Heap[i];
         Heap[j]->index = j;
      }
      UnsortedStart = heapSize;
   }

   struct tri_t *tri;
   for(tri = LastTri; tri != Tris; tri = tri->next)
      AddQTri(tri);
   LastTri = Tris;

   fillpage(x);
   tetrapage(x);

   x += step;
   xi++;

   if(xi >= xnum - 1) {
      /* finished building */
      if(heapMode == HEAP_MIN) {
         SortedPointCount = PointCount;
         SortedTriangleCount = TriangleCount;
         /* once finished building, the heap has to be contiguous */
         buildQHeap();
      } else
         UnsortedRemoveNoTris();
      BuildState = NOTSTARTED;
   }

   MeshModified = 1;

   return xnum - 1 - xi;
}

void meteorFunc(double (*func)(double x, double y, double z))
{
   Func = func;
}

void meteorNormalFunc(void (*func)(double[3], double[3]))
{
   NormalFunc = func;
}

void meteorTexCoordFunc(void (*func)(double[3], double[3]))
{
   TexCoordFunc = func;
}

void meteorColorFunc(void (*func)(double[3], double[3]))
{
   ColorFunc = func;
}
