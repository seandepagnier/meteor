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

/* This file contains routines for allocating the meteor data structure.
   It can use libgc and GC_malloc if supported, or it will keep free
   lists when items are freed, and use malloc when the freelist is
   empty to allocate */

#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

int DataParts; /* number of additional triples of data per point */

#ifdef HAVE_LIBGC
#include <gc.h>
#else
/* different size points */
static struct point_t *FreePoints[4];

static struct trilist_t *FreeTriLists;
static struct tri_t *FreeTris;
#endif

void FreeTriList(struct point_t *p)
{
#ifndef HAVE_LIBGC
   /* get tail pointer */
   struct trilist_t **l;
   for(l = &p->tris; *l; l = &(*l)->next);    /* this makes things slow */
   *l = FreeTriLists;
   FreeTriLists = p->tris;
   p->tris = NULL;
#endif
}

void FreeTri(struct tri_t *tri)
{
   tri->prev->next = tri->next;
   tri->next->prev = tri->prev;
#ifndef HAVE_LIBGC
   tri->next = FreeTris;
   FreeTris = tri;
#endif
   FreedTriangles++;
}

/* remove item at *l, if this empties a circular list, *l points to NULL */
void FreeTriListItem(struct trilist_t **l)
{
   struct trilist_t *m = *l;
   *l = m->next;
#ifndef HAVE_LIBGC
   m->next = FreeTriLists;
   FreeTriLists = m;
#endif
}

void FreePoint(struct point_t *t)
{
#ifndef HAVE_LIBGC
   t->next = FreePoints[DataParts];
   FreePoints[DataParts] = t;
#endif
   t->index = -1; // checks this for points not in the heap for aggregation
   FreedPoints++;
}

struct point_t *AllocPoint(void)
{
   struct point_t *p;
   unsigned int size = sizeof(*p) + 3 * DataParts * sizeof(*p->data);
#ifdef HAVE_LIBGC
   p = GC_malloc(size);
#else
   if(FreePoints[DataParts]) {
      p = FreePoints[DataParts];
      FreePoints[DataParts] = p->next;
   } else
      p = malloc(size);
#endif
   CreatedPoints++;

   return p;
}

struct trilist_t *AllocTriList(void)
{
   struct trilist_t *l;
#ifdef HAVE_LIBGC
   l = GC_malloc(sizeof *l);
#else
   if(FreeTriLists) {
      l = FreeTriLists;
      FreeTriLists = l->next;
   } else
      l = malloc(sizeof *l);
#endif

   return l;
}

struct tri_t *AllocTri(void)
{
   struct tri_t *t;
#ifdef HAVE_LIBGC
   t = GC_malloc(sizeof *t);
#else
   if(FreeTris) {
      t = FreeTris;
      FreeTris = t->next;
   } else
      t = malloc(sizeof *t);

#endif
   CreatedTriangles++;

   t->prev = Tris->prev;
   t->next = Tris;
   Tris->prev = t;
   t->prev->next = t;

   return t;
}

void freePoints(void)
{
   if(!PointCount)
      return;

#ifndef HAVE_LIBGC
   int i = 0;
   for(;;) {
      struct point_t *p = Heap[i];
      FreeTriList(p);
      if(i == PointCount - 1)
         break;
      p->next = Heap[i+1];
      i++;
   }
   Heap[i]->next = FreePoints[DataParts];
   FreePoints[DataParts] = Heap[0];
#endif

   heapSize = 0;
   heapMode = HEAP_NONE;
   CreatedPoints = FreedPoints = 0;
}

/* free all points, tris, and trilists in the meteor */
void freeMem(void)
{
   freePoints();

#ifndef HAVE_LIBGC
   if(Tris->prev != Tris) {
      Tris->prev->next = FreeTris;
      FreeTris = Tris->next;
   }
#endif

   Tris->prev = Tris->next = Tris;
   CreatedTriangles = FreedTriangles = 0;

}

void relinquishMem(void)
{
#ifndef HAVE_LIBGC
#define FREE_LIST(X, Y) while(X) Y = X, X = X->next, free(Y)
   int i;
   for(i = 0; i<4; i++) {
      struct point_t *p = FreePoints[i], *q;
      //      FREE_LIST(p, q);
      int c;
      while(p) {
         q = p;
         p = p->next;
         free(q);
         c++;
         if(c > 2700)
            c= 10000;
      }
      FreePoints[i] = NULL;
   }

   struct trilist_t *l = FreeTriLists, *m;
   FREE_LIST(l, m);
   FreeTriLists = NULL;

   struct tri_t *t = FreeTris, *s;
   FREE_LIST(t, s);
   FreeTris = NULL;
#endif
}
