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

/* kd tree to aid in calculating nearest neighbors */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "internal.h"

#include "linalg.h"

#define DELTA 1.0 /* set to 1.0 for zero error,
                     higher for more error and speed */
#define INF (1.0 / 0.0)

static struct point_t *kdTree; /* head of tree */

static const int nextaxis[] = {1, 2, 0};

/* insert a point and find another point that is closest to it,
   if ins is 0, then it is already inserted and looking for other points
   to see if they are closer than the current minimum */
static void insertrec(struct point_t *p, struct point_t **n, int axis, int ins)
{
   struct point_t *m = *n;
   if(!m) {
      if(ins) {
         p->cost = INF;
         *n = p;
         p->kdaxis = axis;
         p->kdl = p->kdr = NULL;
         p->kdp = n;
      }
      return;
   }

   mfloat dist = p->pos[axis] - m->pos[axis];
   mfloat dist_2 = dist*dist*DELTA;
   int naxis = nextaxis[axis];
   if(dist < 0) {
      insertrec(p, &m->kdl, naxis, ins);
      if(p->cost < dist_2)
         return;
      insertrec(p, &m->kdr, naxis, 0);
   } else {
      insertrec(p, &m->kdr, naxis, ins);
      if(p->cost < dist_2)
         return;
      insertrec(p, &m->kdl, naxis, 0);
   }
   
   dist = dist2(p->pos, m->pos);
   if(dist < p->cost) {
      p->cost = dist;
      p->mp = m;
   }
}

/* put a point in the kdtree, and update the point's cost
   and mp to the closest point to it in the tree */
void kdTreeInsert(struct point_t *p)
{
   insertrec(p, &kdTree, 0, 1);
}

struct point_t *findmin(struct point_t *p, int axis)
{
   if(!p)
      return NULL;
   struct point_t *q = findmin(p->kdl, axis);
   if(p->kdaxis != axis) {
      struct point_t *r = findmin(p->kdr, axis);
      if(!q || (r && r->pos[axis] < q->pos[axis]))
         q = r;
   }

   if(!q || p->pos[axis] < q->pos[axis])
      return p;
   return q;
}

/* pull a point out of the kd tree */
void kdTreeRemove(struct point_t *p)
{
   if(!p->kdr) {
      if(!p->kdl) {
         *p->kdp = NULL;
         return;
      }
      p->kdr = p->kdl;
      p->kdr->kdp = &p->kdr;
      p->kdl = NULL;
   }

   struct point_t *q = findmin(p->kdr, p->kdaxis);
   kdTreeRemove(q);
   q->kdaxis = p->kdaxis;
   q->kdl = p->kdl;
   q->kdr = p->kdr;
   q->kdp = p->kdp;

   *q->kdp = q;
   if(q->kdl)
      q->kdl->kdp = &q->kdl;
   if(q->kdr)
      q->kdr->kdp = &q->kdr;
}

void kdTreeUpdate(struct point_t *p)
{
   kdTreeRemove(p);
   kdTreeInsert(p);
}

void kdTreeClear(void)
{
   kdTree = NULL;
}
