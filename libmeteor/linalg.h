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

static inline void add4x4tri(mfloat x[10], mfloat a[10])
{
   x[0] += a[0], x[1] += a[1], x[2] += a[2], x[3] += a[3];
                 x[4] += a[4], x[5] += a[5], x[6] += a[6];
                               x[7] += a[7], x[8] += a[8];
                                             x[9] += a[9];
}

static inline void add4x4tri3(mfloat x[10], mfloat a[10], mfloat b[10])
{
   x[0] += a[0] + b[0], x[1] += a[1] + b[1], x[2] += a[2] + b[2];
   x[3] += a[3] + b[3], x[4] += a[4] + b[4], x[5] += a[5] + b[5];
   x[6] += a[6] + b[6], x[7] += a[7] + b[7], x[8] += a[8] + b[8];
   x[9] += a[9] + b[9];
}

static void normalize(mfloat n[3])
{
   mfloat d = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
   /* protect against normalizing the zero vector and giving nans */
   if(d > 0) {
      d = 1/d;
      n[0] *= d;
      n[1] *= d;
      n[2] *= d;
   }
}

static mfloat tripleprodtri(mfloat v[3], mfloat a[10])
{
   return v[0]*v[0]*a[0] + v[1]*v[1]*a[4] + v[2]*v[2]*a[7] + a[9]
      + 2*(v[0]*v[1]*a[1] + v[0]*v[2]*a[2] + v[0]*a[3] +
           v[1]*v[2]*a[5] + v[1]*a[6] + v[2]*a[8]);
}

static int solvespecial(mfloat x[3], mfloat A[10])
{
   mfloat a = A[0], b = A[1], c = A[2], d = A[3], e = A[4];
   mfloat f = A[5], g = A[6], h = A[7], i = A[8];
   // mfloat D = c*c*e - 2*b*c*f + a*f*f + b*b*h - a*e*h;
      mfloat D = e*(c*c - a*h) + f*(a*f - 2*b*c) + b*b*h;

   if(fabs(D) < .000001 || fabs(D) > 1000000)
      return 1;

   D = 1 / D;

#if 0
   x[0] = (-d*f*f + c*f*g + d*e*h - b*g*h - c*e*i + b*f*i) * D;
   x[1] = (c*d*f - c*c*g - b*d*h + a*g*h + b*c*i - a*f*i) * D;
   x[2] = (-c*d*e + b*d*f + b*c*g - a*f*g - b*b*i + a*e*i) * D;
#else
   x[0] = (f*(c*g -d*f) + h*(d*e - b*g) + i*(b*f - c*e)) * D;
   x[1] = (c*(d*f - c*g) + h*(a*g - b*d) + i*(b*c - a*f)) * D;
   x[2] = (d*(b*f - c*e) + b*(c*g - b*i) + a*(e*i - f*g)) * D;
#endif
   return 0;
}

static inline void set3(mfloat x[3], mfloat a[3])
{
   x[0] = a[0];
   x[1] = a[1];
   x[2] = a[2];
}

static inline void add3(mfloat x[3], mfloat a[3])
{
   x[0] += a[0];
   x[1] += a[1];
   x[2] += a[2];
}

static inline void avg3(mfloat x[3], mfloat a[3], mfloat b[3])
{
   x[0] = (a[0] + b[0]) / 2;
   x[1] = (a[1] + b[1]) / 2;
   x[2] = (a[2] + b[2]) / 2;
}

static inline void avg33(mfloat x[3], mfloat a[3], mfloat b[3], mfloat c[3])
{
   x[0] = (a[0] + b[0] + c[0]) / 3;
   x[1] = (a[1] + b[1] + c[1]) / 3;
   x[2] = (a[2] + b[2] + c[2]) / 3;
}

static inline void sub3(mfloat x[3], mfloat a[3], mfloat b[3])
{
   x[0] = a[0] - b[0];
   x[1] = a[1] - b[1];
   x[2] = a[2] - b[2];
}

static inline void cross(mfloat x[3], mfloat a[3], mfloat b[3])
{
   x[0] = a[1] * b[2] - a[2] * b[1];
   x[1] = a[2] * b[0] - a[0] * b[2];
   x[2] = a[0] * b[1] - a[1] * b[0];
}

static inline mfloat dot(mfloat a[3], mfloat b[3])
{
   return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

#include <string.h>

static mfloat inverse4(mfloat x[16], mfloat a[16])
{
   #define SWAP(a, b)   {t = a; a = b; b = t;}

   int i, j, k;
   mfloat max, t, det, pivot;
   mfloat b[16];
   static const mfloat identity[16] = {1, 0, 0, 0,
                                       0, 1, 0, 0,
                                       0, 0, 1, 0,
                                       0, 0, 0, 1};

   /*---------- forward elimination ----------*/

   /* put identity matrix in B */
   memcpy(x, identity, sizeof identity);
   memcpy(b, a, sizeof identity);

    det = 1.0;
    for (i=0; i<4; i++) {               /* eliminate in column i, below diag */
        max = -1.;
        for (k=i; k<4; k++)             /* find pivot for column i */
            if (fabs(b[k*4+i]) > max) {
                max = fabs(b[k*4+i]);
                j = k;
            }
        if (max<=0.) return 0.;         /* if no nonzero pivot, PUNT */
        if (j!=i) {                     /* swap rows i and j */
            for (k=i; k<4; k++)
                SWAP(b[i*4+k], b[j*4+k]);
            for (k=0; k<4; k++)
                SWAP(x[i*4+k], x[j*4+k]);
            det = -det;
        }
        pivot = b[i*4+i];
        det *= pivot;
        for (k=i+1; k<4; k++)           /* only do elems to right of pivot */
            b[i*4+k] /= pivot;
        for (k=0; k<4; k++)
            x[i*4+k] /= pivot;
        /* we know that b[i*4+i] will be set to 1, so don't bother to do it */

        for (j=i+1; j<4; j++) {         /* eliminate in rows below i */
            t = b[j*4+i];                /* we're gonna zero this guy */
            for (k=i+1; k<4; k++)       /* subtract scaled row i from row j */
                b[j*4+k] -= b[i*4+k]*t;   /* (ignore k<=i, we know they're 0) */
            for (k=0; k<4; k++)
                x[j*4+k] -= x[i*4+k]*t;
        }
    }

    /*---------- backward elimination ----------*/

    for (i=4-1; i>0; i--) {             /* eliminate in column i, above diag */
        for (j=0; j<i; j++) {           /* eliminate in rows above i */
            t = b[j*4+i];                /* we're gonna zero this guy */
            for (k=0; k<4; k++)         /* subtract scaled row i from row j */
                x[j*4+k] -= x[i*4+k]*t;
        }
    }

    return det;
}

static inline mfloat dist2(mfloat a[3], mfloat b[3])
{
   mfloat x[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
   return x[0]*x[0] + x[1]*x[1] + x[2]*x[2];
}

static inline mfloat dist(mfloat a[3], mfloat b[3])
{
   return sqrt(dist2(a, b));
}

/* conpute v*a*v efficiently */
static mfloat tripleprod(mfloat v[3], mfloat a[16])
{
   return v[0]*v[0]*a[0] + v[1]*v[1]*a[5] + v[2]*v[2]*a[10] + a[15]
      + 2*(v[0]*v[1]*a[1] + v[0]*v[2]*a[2] + v[0]*a[3] +
           v[1]*v[2]*a[6] + v[1]*a[7] + v[2]*a[11]);
}

static inline void lininterpolate3(mfloat x[3], mfloat a[3], mfloat b[3], mfloat pos)
{
   x[0] = a[0] + pos*(b[0] - a[0]);
   x[1] = a[1] + pos*(b[1] - a[1]);
   x[2] = a[2] + pos*(b[2] - a[2]);
}

/* iteratively improve the location of the point */
static inline void iterativeimprove(mfloat pos[3], mfloat q1[4], mfloat q2[4],
                                    double func(double, double, double))
{
   int i;
   for(i = 0;; i++) {
      lininterpolate3(pos, q1, q2, fabs(q1[3]) / fabs(q1[3] - q2[3]));
      if(i == 5) /* it gets pretty damn close at 5 */
	 break;
      mfloat v = func(pos[0], pos[1], pos[2]);

      if(v * q1[3] >= 0)
	 q1[0] = pos[0], q1[1] = pos[1], q1[2] = pos[2], q1[3] = v;
      else
	 q2[0] = pos[0], q2[1] = pos[1], q2[2] = pos[2], q2[3] = v;
   }
}
