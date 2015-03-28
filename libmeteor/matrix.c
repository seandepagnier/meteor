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

#include <math.h>
#include "meteor.h"
#include "internal.h"

#include "linalg.h"

#define DEG2RAD (M_PI / 180.0)

void meteorMultMatrix(double m[16])
{
   int i;
   for(i = 0; i<PointCount; i++) {
      struct point_t *p = Heap[i];
      mfloat v[3] = {p->pos[0], p->pos[1], p->pos[2]};
      p->pos[0] = v[0]*m[0] + v[1]*m[1] + v[2]*m[2] + m[3];
      p->pos[1] = v[0]*m[4] + v[1]*m[5] + v[2]*m[6] + m[7];
      p->pos[2] = v[0]*m[8] + v[1]*m[9] + v[2]*m[10] + m[11];
      //      p->pos[3] = v[0]*m[12] + v[1]*m[13] + v[2]*m[14] + v[3]*m[15];

      /* update normal, but no translation, only rotation */
      if(DataFormat & METEOR_NORMALS) {
         mfloat *n = p->data+NormalOffset;
         mfloat nv[3] = {n[0], n[1], n[2]};         
         n[0] = nv[0]*m[0] + nv[1]*m[1] + nv[2]*m[2];
         n[1] = nv[0]*m[4] + nv[1]*m[5] + nv[2]*m[6];
         n[2] = nv[0]*m[8] + nv[1]*m[9] + nv[2]*m[10];
      }
   }
}

/* code came from mesa */
void meteorRotate(double angle, double x, double y, double z)
{
   double m[16];
   double xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c, s, c;

   s = sin( angle * DEG2RAD );
   c = cos( angle * DEG2RAD );

   double mag = sqrt(x * x + y * y + z * z);
   if(mag <= 10e-4)
      return;

   x /= mag;
   y /= mag;
   z /= mag;

   xx = x * x;
   yy = y * y;
   zz = z * z;
   xy = x * y;
   yz = y * z;
   zx = z * x;
   xs = x * s;
   ys = y * s;
   zs = z * s;
   one_c = 1.0F - c;

   m[0] = (one_c * xx) + c;
   m[1] = (one_c * xy) - zs;
   m[2] = (one_c * zx) + ys;
   m[3] = 0;

   m[4] = (one_c * xy) + zs;
   m[5] = (one_c * yy) + c;
   m[6] = (one_c * yz) - xs;
   m[7] = 0;

   m[8] = (one_c * zx) - ys;
   m[9] = (one_c * yz) + xs;
   m[10] = (one_c * zz) + c;
   m[11] = 0.0F;

   m[12] = 0.0F;
   m[13] = 0.0F;
   m[14] = 0.0F;
   m[15] = 1.0F;

   meteorMultMatrix(m);
}

void meteorTranslate(double x, double y, double z)
{
   double m[16] = {1, 0, 0, x,
                   0, 1, 0, y,
                   0, 0, 1, z,
                   0, 0, 0, 1};
   meteorMultMatrix(m);
}

void meteorScale(double x, double y, double z)
{
   double m[16] = {x, 0, 0, 0,
                   0, y, 0, 0,
                   0, 0, z, 0,
                   0, 0, 0, 1};
   meteorMultMatrix(m);
}
