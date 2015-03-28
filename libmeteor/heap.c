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
#include "internal.h"

#define min(x, y) (x < y ? x : y)

static int maxsize; /* number of elements currently allocated for space */
struct point_t **Heap; /* heap data, packed binary tree */

int heapSize; /* size of sorted data, there are always PointCount in the heap */
int heapMode; /* what the heap is currently used for */

static inline int parent(int a) {
   return  (a-!(a&1)) >> 1;
}

static inline int child(int a) {
   return (a<<1) + 1;
}

void heapInsertUnsorted(struct point_t *p) {
   /* make sure we have enough storage */
   if(PointCount > maxsize) {
      maxsize = PointCount*2;
      Heap = realloc(Heap, maxsize * (sizeof *Heap));
   }

   p->index = PointCount - 1;
   Heap[PointCount - 1] = p;
}

void heapInsert(struct point_t *p) {
   int n = heapSize++;

   for(;;) {
      int o = parent(n);
      if(n == 0 || Heap[o]->cost <= p->cost) {
         p->index = n;
	 Heap[n] = p;
	 break;
      }

      Heap[n] = Heap[o];
      Heap[n]->index = n;
      n = o;
   }
}

void heapRemove(struct point_t *p)
{
   int c, o;
#ifdef DEBUG
   if(heapSize == 0)
      die("cannot remove from empty heap\n");
#endif

   heapSize--;

   /* if p->index is -1 then p has been deleted, there is a bug somewhere else */
   o = p->index;
   c = child(o);
   while(c < heapSize && min(Heap[c]->cost, Heap[c+1]->cost) < Heap[heapSize]->cost) {
      if(Heap[c]->cost > Heap[c+1]->cost)
	 c++;

      Heap[o] = Heap[c];
      Heap[o]->index = o;

      o = c;
      c = child(c);
   }

   int par = parent(o);
   while(o > 0 && Heap[par]->cost > Heap[heapSize]->cost) {
      Heap[o] = Heap[par];
      Heap[o]->index = o;

      o = par;
      par = parent(par);
   }

   Heap[o] = Heap[heapSize];
   Heap[o]->index = o;
}

void heapUpdate(struct point_t *p)
{
   heapRemove(p);
   heapInsert(p);
}
