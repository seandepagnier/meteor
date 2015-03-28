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
#include <string.h>
#include <math.h>

#include "internal.h"
#include "meteor.h"
#include "linalg.h"

/* globals from this file */
void (*CalculateOptimalPoint)(struct point_t *p, int init);

unsigned int CreatedPoints, FreedPoints;
unsigned int CreatedTriangles, FreedTriangles;
unsigned int SortedTriangleCount;

int DataFormat = METEOR_COORDS;
int NormalOffset, ColorOffset, TexCoordOffset;

static struct tri_t header = {&header,&header};
struct tri_t *Tris = &header;

static inline mfloat CalculateQuadricContractionCost(mfloat q1[10], mfloat q2[10])
{
   mfloat A = q1[0]+q2[0], B = q1[1]+q2[1], C = q1[2]+q2[2];
   mfloat D = q1[3]+q2[3], E = q1[4]+q2[4], F = q1[5]+q2[5];
   mfloat G = q1[6]+q2[6], H = q1[7]+q2[7], I = q1[8]+q2[8];
   mfloat J = q1[9]+q2[9];

#if 1  /* use optimized version (see term-optimizer.scm) */
   double t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    t0 = (B * I);
    t1 = (C * G);
    t2 = (D * F);
    t3 = (A * F);
    t4 = (B * H);
    t5 = (C * E);
    t6 = (A * E);
    t7 = (-2 * D);
    t8 = (2 * t0);
    t9 = (((F * t3) + (-2 * B * C * F) + (B * t4) + (C * t5)) - (t6 * H));

   if(t9 == 0)
      return 1.0/0.0;

    return (J*t9 + I*I*t6 + t3*-2*G*I + t8*t1 + t8*t2 + t7*t5*I + G*H*A*G
            + t7*t4*G + D*H*D*E + t2*t1*2 - t0*t0 - t1*t1 - t2*t2) / t9;
#else
    /* unoptimized */
    /* this is value for x where x=Q^-1*v*Q where Q = q1+q2.
       v is the third column of R^-1 where R is Q with the last row
       of 0 0 0 1 */
    /* Solve[{a*x+b*y+c*z+d==0, b*x+e*y+f*z+g==0, c*x+f*y+h*z+i==0,
       cost==x*x*a+y*y*e+z*z*h+j+2*(x*y*b+x*z*c+x*d+y*z*f+y*g+z*i)}, {x,y,z,cost}] */

   mfloat denom = (C*C*E - 2*B*C*F + A*F*F + B*B*H - A*E*H);
   if(denom == 0)
      return 1.0/0.0;

   return (-D*D*F*F + 2*C*D*F*G - C*C*G*G + D*D*E*H - 2*B*D*G*H +
           A*G*G*H - 2*C*D*E*I + 2*B*D*F*I + 2*B*C*G*I - 2*A*F*G*I -
           B*B*I*I + A*E*I*I + C*C*E*J - 2*B*C*F*J + A*F*F*J + B*B*H*J
           - A*E*H*J) / denom;
#endif
}

/* if init is set, then only half the connections are scanned
   (so we don't test a to b as well as b to a) */
void CalculateQuadricPoint(struct point_t *p, int init)
{
   mfloat minc = 1.0/0.0;
   struct point_t *minp = NULL;
   struct trilist_t *l;

   for(l = p->tris; l; l=l->next) {
      struct tri_t *tri = l->tri;
      int ni;
      struct point_t *np;
      
      if(tri->p[0] == p)
         ni = 0;
      else if(tri->p[1] == p)
         ni = 1;
      else
         ni = 2;

      np = tri->p[(ni+1)%3];

      /* optimization to cut initial calculations in half (get updated
         below anyway) */
      if(init) {
         if(p->index < np->index)
            continue; 
      } else
      /* in the process of building, and the other point isn't in the heap */
         if(np->index >= heapSize)
            continue;

      mfloat c = CalculateQuadricContractionCost(p->Q, np->Q);

#if 1 /* optional step, appears to improve quality (small slowdown) */
      if(np->cost > c) {
         np->cost = c;
         np->mp = p;
         heapUpdate(np);
      }
#endif
      /* update the other's cost */
      else if(np->mp == p) {
         np->cost = c;
         heapUpdate(np); /* optional, improves quality (large slowdown) */
      }

      /* must have <=, minc starts at inf, the min cost might be inf */
      if(c <= minc) {  
         minc = c;
         minp = np;
      }
   }

   p->cost = minc;
   p->mp = minp;
}

/* attempt to calculate contraction cost based on
   linear algebra that takes edges into account, not currently used */
void CalculateEdgePoint(struct point_t *p, int init)
{
   mfloat minc = 1.0/0.0;
   struct point_t *minp = NULL;
   struct trilist_t *l;

   mfloat inc=0;
   for(l = p->tris; l; l=l->next) {
      struct tri_t *tri = l->tri;
      int ni;
      struct point_t *np, *op;
      
      if(tri->p[0] == p)
         ni = 0;
      else if(tri->p[1] == p)
         ni = 1;
      else
         ni = 2;

      np = tri->p[(ni+1)%3];
      op = tri->p[(ni+2)%3];

      mfloat vec[3][3];
      sub3(vec[0], p->pos, np->pos);
      sub3(vec[1], p->pos, op->pos);

      mfloat cr[3];
      cross(cr, vec[0], vec[1]);

      struct trilist_t *m;
      for(m = p->tris; m; m=m->next) {
         struct tri_t *tri2 = m->tri;

         int i;
         for(i = 0; i<3; i++) {
            struct point_t *mp = tri2->p[i];
            if(mp == p || mp == np || mp == op)
               continue;
            sub3(vec[2], p->pos, mp->pos);
            mfloat fac = dot(cr, vec[2]) / dot(cr, cr);
            cr[0] *= fac, cr[1] *= fac, cr[2] *= fac;
            sub3(vec[2], vec[2], cr);

            vec[2][0] = -vec[2][0], vec[2][1] = -vec[2][1], vec[2][2] = -vec[2][2];
            double x = dot(vec[0], vec[1]);
            double y = dot(vec[0], vec[2]);
            double z = dot(vec[1], vec[2]);

            inc++;
            if(y < x && z < x) {
               double c = dot(vec[2], vec[2]) ;
               if(c <= minc) {  
                  minc = c;
                  minp = mp;
               }
            }
         }
      }
   }

   p->cost = minc;
   p->mp = minp;
}

double meteorPropagate(int iterations)
{
   struct point_t *p;
   mfloat improvement = 0;
   mfloat num = 0;
   int i;
   for(i = 0; i<PointCount; i++) {
      struct point_t *p = Heap[i];
      mfloat *pos = p->pos;
      mfloat n[3];

#ifdef USE_DOUBLE_FORMAT
      NormalFunc(n, pos);
#else
      double dn[3], dpos[3] = {pos[0], pos[1], pos[2]};
      NormalFunc(dn, dpos);
      n[0] = dn[0], n[1] = dn[1], n[2] = dn[2];
#endif
      normalize(n);
      mfloat val = Func(pos[0], pos[1], pos[2]);
      mfloat startval = val;
      int i;
      mfloat step = val;
      if(val == 0)
         continue;

      for(i = 0; i < iterations; i++) {
         mfloat newpos[3] = {pos[0] + step*n[0],
                             pos[1] + step*n[1], pos[2] + step*n[2]};
         
         mfloat newval = Func(newpos[0], newpos[1], newpos[2]);
         if(fabs(val) < fabs(newval)) {
            if(val*newval < 0)
               step *= .5;
            else 
               step *= -.5;
         } else {
            if(val*newval < 0)
               step *= -.3;
            else
               step *= 1.2;
            val = newval;
            memcpy(pos, newpos, sizeof newpos);
         }
         if(newval == 0)
            break;
      }
      improvement += (fabs(startval)-fabs(val))/fabs(startval);
      num++;
   }
   return improvement/num;
}

static void removetrilistitem(struct point_t *p, struct tri_t *tri)
{
   struct trilist_t **l = &p->tris;
   while(*l) {
      if((*l)->tri == tri) {
         FreeTriListItem(l);
         return;
      }
      l=&(*l)->next;
   }
   die("couldn't find a triangle to remove\n");
}

/* adds a tri the the trilist of a point */
void addToTriList(struct point_t *p, struct tri_t *q)
{
   struct trilist_t *List = AllocTriList();
   List->next = p->tris;
   List->tri = q;
   p->tris = List;
}

static inline void callfunc(void (*func)(double [3], double [3]),
                            int Offset, struct point_t *p1,
                            struct point_t *p2, struct point_t *p3)
{
   if(func) {
#ifdef USE_DOUBLE_FORMAT
      func(p1->data + Offset, p1->pos);
#else
      double data[3], dpos[3] = {p1->pos[0], p1->pos[1], p1->pos[2]};
      func(data, dpos);
      p1->data[Offset+0] = data[0];
      p1->data[Offset+1] = data[1];
      p1->data[Offset+2] = data[2];
#endif
      } else
         avg3(p1->data+Offset, p2->data+Offset, p3->data+Offset);
}

static inline void updateextra(struct point_t *p1, struct point_t *p2, struct point_t *p3)
{
   if(DataFormat & METEOR_NORMALS)
      callfunc(NormalFunc, NormalOffset, p1, p2, p3);
   if(DataFormat & METEOR_COLORS)
      callfunc(ColorFunc, ColorOffset, p1, p2, p3);
   if(DataFormat & METEOR_TEXCOORDS)
      callfunc(TexCoordFunc, TexCoordOffset, p1, p2, p3);
}

/* this is a generic algorithm that takes the least cost point out of the heap,
   and merges it with the point calculated to be the least cost to merge.  It
   also updates all of the data structures involved for susequent operations */
static inline void MergeTopOfHeap(int kd)
{
 restart:
   if(heapSize < 2)
      return;

   struct point_t *p2 = Heap[0], *p1 = p2->mp;

   /* The nearest neighbor might not be connected in a triangle (aggregation)
      making it difficult to efficiently detect which points saw the removed
      point as a nearest neighbor, so it's possible the current point
      has a deleted nearest neighbor, recalculate it */
   if(kd) {
      if(p1->index == -1) {
         kdTreeUpdate(p2);
         p1 = p2->mp;
      }
   } else {
      /* for quadric heap, only merges to points that share triangles
         is allowed, check to make sure this point still shares a triangle
         it could have been removed in the mean time */
      /* consider always calculating here, and not storing mp?? */
      struct trilist_t *v;
      for(v = p2->tris; v; v=v->next) {
         int i;
         for(i = 0; i<3; i++)
            if(v->tri->p[i] == p1)
               goto haveit;
      }
      /* the precalculated mp is missing, recalculate it and update
         the heap then try again */
      CalculateOptimalPoint(p2, 0);
      heapUpdate(p2);
      goto restart;
   }
 haveit:

#ifdef DEBUG
   if(p1->index >= heapSize)
      die("invalid merge attempt\n");

   if(p1 == p2)
      die("p1 == p2\n");
#endif

   if(kd) {
      /* always average position for aggregation */
      avg3(p1->pos, p1->pos, p2->pos);
   } else {
      /* set add p2's q matrix to p1's q matrix */
      add4x4tri(p1->Q, p2->Q);
      /* set p1's position to the calculated position, if it can't
         be calculated with quadrics, just average the two points */
      if(solvespecial(p1->pos, p1->Q))
         avg3(p1->pos, p1->pos, p2->pos);
   }

   /* update extra data for the point if it exists */
   updateextra(p1, p1, p2);

   /* pull p2 out of the heap */
   heapRemove(p2);
   if(kd)
      kdTreeRemove(p2);

   /* go through all of p2's triangles, and remove any that touch p1,
      including removing this triangle from each of its points' lists.
      otherwise update the triangle connected to p2 to connect to p1 instead */
   struct trilist_t **t = &p2->tris;
   while(*t) {
      struct tri_t *rmtri = (*t)->tri;
      int i;
      if(rmtri->p[0] == p1 || rmtri->p[1] == p1 || rmtri->p[2] == p1) {
         for(i = 0; i < 3; i++) {
            struct point_t *p = rmtri->p[i];
            if(p != p2) {
               removetrilistitem(p, rmtri);

               if(p != p1) {
                  /* if we remove all the triangles from this point, and it isn't
                     p1 or p2, then remove the point */
                  if(!p->tris) {
                     /* don't remove it if it isn't in the heap yet */
                     if(p->index <= heapSize) {
                        heapRemove(p);
                        if(kd)
                           kdTreeRemove(p);
                        FreePoint(p);
                     }
                  }
               }
            }
         }
         /* remove the triangle from p2's list */
         FreeTriListItem(t);

         /* free the triangle */
         FreeTri(rmtri);
      } else {
         /* update all triangles attached to p2 to be attached to p1 instead */
         for(i = 0; i < 3; i++)
            if(rmtri->p[i] == p2)
               goto ok;

         die("did not find p2 in triangle in it's trilist\n");
      ok:
         rmtri->p[i] = p1;
         t = &(*t)->next;
      }
   }

   /* join p1 and p2's trilists and give it to p1, delete p2
      since it is now merged with p1 */
   *t = p1->tris;
   p1->tris = p2->tris;

   /* removed above, now we can free */
   FreePoint(p2);

   /* if p1 has any triangles, update the cost for p1, otherwise delete p1 */
   if(p1->tris) {
      if(kd)
         kdTreeUpdate(p1);
      else
         CalculateOptimalPoint(p1, 0);
      heapUpdate(p1);
   } else {
      heapRemove(p1);
      if(kd)
         kdTreeRemove(p1);
      FreePoint(p1);
   }
}

/* perform a pair contraction, and return the number of triangles removed */
int meteorMerge(void)
{
   /* make sure the points are sorted based on contraction cost */
   buildQHeap();

   /* keep track of initial triangle count */
   int num = TriangleCount;

   MergeTopOfHeap(0);

   MeshModified = 1;
   int diff = num - TriangleCount;
   SortedTriangleCount -= diff;
   return diff;
}

/* join the two points that are closest together */
int meteorAggregate(void)
{
   if(!PointCount)
      return 0;

   if(heapMode != HEAP_AGGREGATE) {
      kdTreeClear();
      int i;
      for(i = 0; i < PointCount; i++)
         kdTreeInsert(Heap[i]);
      heapSize = 0;
      for(i = 0; i<PointCount; i++)
         heapInsert(Heap[i]);
      heapMode = HEAP_AGGREGATE;
   }

   /* keep track of initial point count */
   int num = PointCount;

   MergeTopOfHeap(1);

   MeshModified = 1;
   return num - PointCount;
}

/* cut the edge with points p1, and p2, and use p as the intermediate point */
static void sliceedge(struct point_t *p1, struct point_t *p2, struct point_t *p)
{
   /* split all triangles that cross this boundary */
   struct trilist_t *l;
   for(l = p1->tris; l; l = l->next) {
      struct tri_t *stri = l->tri;
      int p1i, p2i = -1, p3i;
      int i;
      for(i = 0; i < 3; i++)
         if(stri->p[i] == p1)
            p1i = i;
         else
            if(stri->p[i] == p2)
               p2i = i;
            else
               p3i = i;

      /* we only care about triangles that touch p1 and p2 */
      if(p2i == -1)
         continue;
      
      struct point_t *p3 = stri->p[p3i];
      struct tri_t *ntri = AllocTri();
      
      ntri->p[0] = p1;
      /* get the order of the new triangle right */
      if((p1i - p2i + 3)%3 == 2)
         ntri->p[1] = p, ntri->p[2] = p3;
      else
         ntri->p[2] = p, ntri->p[1] = p3;

      /* shift this triangle to use the new point instead of p1 */
      stri->p[p1i] = p;

      /* update the current trilist to use the new triangle */
      l->tri = ntri;
      
      /* add these triangles to the appropriate point's tri lists */
      addToTriList(p, stri);
      addToTriList(p, ntri);
      addToTriList(p3, ntri);
   }
}

/* cut the meteor like a knife by splitting all edges that cut
   the given equation */
static void slicemeteor(double (*func)(double, double, double))
{
   int i;
   for(i = 0; i<PointCount; i++) {
      struct point_t *p = Heap[i];
      p->cut = func(p->pos[0], p->pos[1], p->pos[2]);
   }

   /* for each triangle, split it into 3 pieces if needed */
   struct tri_t *tri;
   for(tri = Tris->next; tri != Tris; tri = tri->next) {
      int i, j;
      struct point_t *p1, *p2;
      for(i = 2, j = 0; j < 3; i = j, j++) {
         p1 = tri->p[i], p2 = tri->p[j];
         if(p1->cut * p2->cut < 0) {
            struct point_t *p = NewPoint();
            
            /* calculate new position, iterativly move it closer
               to the cutting equation */
            mfloat q1[4] = {p1->pos[0], p1->pos[1], p1->pos[2], p1->cut};
            mfloat q2[4] = {p2->pos[0], p2->pos[1], p2->pos[2], p2->cut};
            
            iterativeimprove(p->pos, q1, q2, func);

            /* the cut must be 0 even if it
               isn't perfectly on the clipping func */
            p->cut = 0;

            /* update q matrix */
            add4x4tri3(p->Q, p1->Q, p2->Q);
            
            /* update extra data for the point if it exists */
            updateextra(p, p1, p2);

            /* split the edge */
            sliceedge(p1, p2, p);
         }
      }
   }
}

/* remove parts of the meteor where the function provided is negative,
   and create edges along 0, cutting then clipping is less optimal
   than a specialized clipping routine, therefore this is not optimized
   This operation is O(n). */
void meteorClip(double (*func)(double, double, double))
{
   slicemeteor(func);

   int i, j;
   /* go throught points, delete points that are negative cuts */
   for(i = 0; i < PointCount; i++) {
      struct point_t *p = Heap[i];
      if(p->cut < 0) {
         FreeTriList(p);
         FreePoint(p);
         
         while(PointCount > i && Heap[PointCount]->cut < 0)
            FreePoint(Heap[PointCount]);

         Heap[i] = Heap[PointCount];
         Heap[i]->index = i;
      }
   }

   /* remove triangles that are on the negative side of the cut */
   struct tri_t *ftri = Tris->next;
   while(ftri != Tris) {
      struct tri_t *tri = ftri;
      ftri = ftri->next;
      for(j = 0; j < 3; j++)
         if(tri->p[j]->cut < 0) {
            FreeTri(tri);
            tri->p[0] = NULL;
            goto freed;
         }
   freed:;
   }

   /* this point is on the edge, we need to cut links * to removed tr */
   for(i = 0; i < PointCount; i++) {
      struct trilist_t **l = &Heap[i]->tris;
      while(*l) {
         struct tri_t *tri = (*l)->tri;
         if(tri->p[0] == NULL)
            FreeTriListItem(l);
         else
            l = &(*l)->next;
      }
   }

   MeshModified = 1;
}

static const mfloat texcorrecttolerance = .4;

static void correcttexcoordsaxis(int k)
{
   /* make sure all tex coords are between 0 and 1 */
   int i;
   for(i = 0; i < PointCount; i++) {
      struct point_t *p = Heap[i];
      mfloat *pt = p->data + TexCoordOffset;
         
      while(pt[k] >= 1)
         pt[k]--;
      while(pt[k] < 0)
         pt[k]++;

      if(isnan(pt[k]))
         pt[k]=0;
   }

   /* for each triangle, see if any of the edges cross
      a "texture boundary" in this case, make a new point
      and split the edge, give it a texcoord of 0 */
   struct tri_t *tri;
   for(tri = Tris->next; tri != Tris; tri = tri->next) {
      int j;
      struct point_t *p1, *p2;

      for(i = 2, j = 0; j < 3; i = j, j++) {
         p1 = tri->p[i], p2 = tri->p[j];
         mfloat *p1t = p1->data + TexCoordOffset;
         mfloat *p2t = p2->data + TexCoordOffset;

         mfloat tex1 = (p1t[k] > .5) ? (p1t[k] - 1) : p1t[k];
         mfloat tex2 = (p2t[k] > .5) ? (p2t[k] - 1) : p2t[k];

         if(tex1 * tex2 < 0 && fabs(tex1 - tex2) < texcorrecttolerance) {
            struct point_t *p = NewPoint();

            /* calculate new position, don't deal with poles,
               just interpolate (with sane clamping) */
            mfloat mult = fabs(tex1)/fabs(tex1 - tex2);
            lininterpolate3(p->pos, p1->pos, p2->pos, mult);

            /* update q matrix */
            add4x4tri3(p->Q, p1->Q, p2->Q);
            
            /* update extra data for the point if it exists */
            updateextra(p, p1, p2);

            /* mark 0 it for updating later */
            mfloat *pt = p->data + TexCoordOffset;
            pt[k] = 0;

            /* split the edge */
            sliceedge(p1, p2, p);
         }
      }
   }

   /* for each point with a texcoord of 0, make a point with a texcoord of 1 */
   for(i = 0; i<PointCount; i++) {
      struct point_t *p = Heap[i];
      mfloat *pt = p->data + TexCoordOffset;
      if(pt[k] == 0) {
         struct point_t *np = NewPoint();
         /* copy in the data */
         memcpy(np->pos, p->pos, sizeof p->pos);

         memcpy(np->Q, p->Q, sizeof p->Q);
         memcpy(np->data, p->data, 3 * DataParts * sizeof *p->data);

         /* set the new point's texcoord to 1 */
         mfloat *npt = np->data + TexCoordOffset;
         npt[k] = 1;

         /* go through all the triangles, and put the right triangle
            with the right point */
         struct trilist_t **l = &p->tris;
         while(*l) {
            struct trilist_t *m = *l;
            struct tri_t *tri = m->tri;

            int p1i, p2i, p3i;
            for(p1i = 0; p1i < 3; p1i++)
               if(tri->p[p1i] == p)
                  break;
            p2i = (p1i + 1) % 3;
            p3i = (p1i + 2) % 3;

            mfloat *p2t = tri->p[p2i]->data + TexCoordOffset;
            mfloat *p3t = tri->p[p3i]->data + TexCoordOffset;
            if(p2t[k] > .5 || p3t[k] > .5) {
               /* this triangle belongs with np not p */
               tri->p[p1i] = np;
               *l = m->next;
               m->next = np->tris;
               np->tris = m;
            } else
               l=&(*l)->next;
         }

         /* clean up points without triangles */
         if(!p->tris) {
            FreePoint(p);
            Heap[i] = np;
            np->index = i;
         } else
         if(!np->tris)
            FreePoint(np);
      }
   }
}

/* attempt to modify the meteor so it has duplicate located points with
   different texture coordinates. does not handle so-called "poles".  */
void meteorCorrectTexCoords(void)
{
   if(!(DataFormat & METEOR_TEXCOORDS))
      return;

   /* correct for each axis */
   int i;
   for(i = 0; i < 3; i++)
      correcttexcoordsaxis(i);

   MeshModified = 1;
}

static void updateOffsets(void)
{
   NormalOffset = 0;
   ColorOffset = NormalOffset + 3 * !!(DataFormat & METEOR_NORMALS);
   TexCoordOffset = ColorOffset + 3 * !!(DataFormat & METEOR_COLORS);
   DataParts = TexCoordOffset/3 + !!(DataFormat & METEOR_TEXCOORDS);
}

void meteorReset(int format)
{
#if 1
   CalculateOptimalPoint = CalculateQuadricPoint;
#else
   CalculateOptimalPoint = CalculateEdgePoint;
#endif

   freeMem();
   DataFormat = format | METEOR_COORDS;
   updateOffsets();
   meteorRewind();
}

int meteorFormat(void)
{
   return DataFormat;
}

int meteorPointCount(void)
{
   return PointCount;
}

int meteorPointCreatedCount(void)
{
   return CreatedPoints;
}

int meteorTriangleCount(void)
{
   return TriangleCount;
}

int meteorTriangleCreatedCount(void)
{
   return CreatedTriangles;
}

int meteorTriangleMergeableCount(void)
{
   return SortedTriangleCount;
}
