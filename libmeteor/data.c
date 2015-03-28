#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "meteor.h"

int newmeteorerror;
char meteorerror[256] = "";

/* need a version without spaces for macros */
typedef unsigned int unsigned_int;
typedef long double long_double;

/* flag used to determine if the meteor structure 
   has been modified since reading the data */
int MeshModified;

static int curpointind;
static struct tri_t *curtri;
static int lastpointmask, lasttrianglemask;

#define ERROR(x) do { strcpy(meteorerror, __func__); strcat(meteorerror, ": "); \
                   strcat(meteorerror, x); newmeteorerror = 1; return -1; } while(0)

void meteorRewind(void)
{
   lastpointmask = lasttrianglemask = 0;
   curpointind = 0;
   curtri = Tris->next;

   MeshModified = 0;
}

/* tests to make sure the call was valid */
#define TEST_VALID_TYPE \
   if(type != METEOR_INT && type != METEOR_UNSIGNED_INT && type != METEOR_FLOAT \
      && type != METEOR_DOUBLE && type != METEOR_LONG_DOUBLE) \
      ERROR("Invalid type requested")

#define TEST_VALID_FORMAT \
   if(format & ~DataFormat) \
      ERROR("Format not available")

#define TEST_MODIFIED \
   if(MeshModified) \
      ERROR("Mesh has been modified since rewind")

/* the routines that copy point data from the library to the user's buffer,
   they use macros to allow for conversions in each format */
#define MAKE_TAKE_POINTDATA(type) \
static void take_pointdata_##type(type **data, struct point_t *p, int format) \
{ \
   if(format == METEOR_INDEX) { \
      *(*data)++ = p->index; \
      return; \
   } \
   if(format & METEOR_COORDS) \
      TAKE_DATA(p->pos); \
   if(format & METEOR_NORMALS) \
      TAKE_DATA(p->data + NormalOffset); \
   if(format & METEOR_COLORS) \
      TAKE_DATA(p->data + ColorOffset); \
   if(format & METEOR_TEXCOORDS) \
      TAKE_DATA(p->data + TexCoordOffset); \
}

#define TAKE_DATA(x) (*data)[0] = (x)[0], (*data)[1] = (x)[1], (*data)[2] = (x)[2], *data+=3
MAKE_TAKE_POINTDATA(int)
MAKE_TAKE_POINTDATA(unsigned_int)
MAKE_TAKE_POINTDATA(float)
MAKE_TAKE_POINTDATA(double)
MAKE_TAKE_POINTDATA(long_double)

static void (*take_pointdata[])() = {take_pointdata_int,
                                     take_pointdata_unsigned_int,
                                     take_pointdata_float,
                                     take_pointdata_double,
                                     take_pointdata_long_double};

int meteorReadPoints(int count, int format, int type, void *data)
{
   TEST_VALID_TYPE;
   TEST_VALID_FORMAT;
   TEST_MODIFIED;

   int i;
   for(i = 0; i<count; i++) {
      if(lastpointmask & format) {
         lastpointmask = format;
         if(++curpointind == PointCount)
            return i+1;
      } else
         lastpointmask |= format;
      take_pointdata[type](&data, Heap[curpointind], format);

      if(Heap[curpointind]->index != curpointind) /* Sanity Check */
         die("Invalid Point!");
   }
   return i;
}

int meteorReadTriangles(int count, int format, int type, void *data)
{
   TEST_VALID_TYPE;
   TEST_VALID_FORMAT;
   TEST_MODIFIED;

   int newf = format;
   if(newf == METEOR_INDEX)
      newf |= 16;

   int i, j;
   for(i = 0; i<count; i++) {
      if(lasttrianglemask & newf) {
         lasttrianglemask = newf;
         curtri = curtri->next;
      } else
         lasttrianglemask |= newf;

      if(curtri == Tris)
         break;

      for(j = 0; j < 3; j++)
         take_pointdata[type](&data, curtri->p[j], format);
   }
   return i;
}

#define MAKE_PUT_POINTDATA(type) \
static void put_pointdata_##type(type **data, struct point_t *p, int format) \
{ \
   if(format == METEOR_INDEX) { \
      p->index = *(*data)++; \
      return; \
   } \
   if(format & METEOR_COORDS) \
      PUT_DATA(p->pos); \
   if(format & METEOR_NORMALS) \
      PUT_DATA(p->data + NormalOffset); \
   if(format & METEOR_COLORS) \
      PUT_DATA(p->data + ColorOffset); \
   if(format & METEOR_TEXCOORDS) \
      PUT_DATA(p->data + TexCoordOffset); \
}

#define PUT_DATA(x) (x)[0] = (*data)[0], (x)[1] = (*data)[1], (x)[2] = (*data)[2], *data+=3
MAKE_PUT_POINTDATA(int)
MAKE_PUT_POINTDATA(unsigned_int)
MAKE_PUT_POINTDATA(float)
MAKE_PUT_POINTDATA(double)
MAKE_PUT_POINTDATA(long_double)

static void (*put_pointdata[])() = {put_pointdata_int,
                                     put_pointdata_unsigned_int,
                                     put_pointdata_float,
                                     put_pointdata_double,
                                     put_pointdata_long_double};

int meteorWritePoints(int count, int format, int type, const void *data)
{
   struct point_t *offset = NULL;

   TEST_VALID_TYPE;
   TEST_VALID_FORMAT;

   int i;
   for(i = 0; i<count; i++) {
      struct point_t *p;
      if(format & METEOR_COORDS) {
         p = NewPoint(); 
         lastpointmask = 0;
      } else {
         if(lastpointmask & format) {
            lastpointmask = format;
            if(++curpointind == PointCount)
               return i+1;
         } else
            lastpointmask |= format;
         p = Heap[curpointind];
      }
      put_pointdata[type](&data, p, format);
   }

   return count;
}

int meteorWriteTriangles(int count, int format, int type, const void *data)
{
   TEST_VALID_TYPE;
   TEST_VALID_FORMAT;

   if(!(format & METEOR_COORDS) && format != METEOR_INDEX)
      ERROR("Must have coordinate or index data when creating triangles\n");

   char cp[3][sizeof(struct point_t) + 3 * DataParts * sizeof(mfloat)];
   struct point_t *points[3] = {(struct point_t*)cp[0],
                                (struct point_t*)cp[1],
                                (struct point_t*)cp[2]};
   int i, j;
   for(i = 0; i<count; i++) {
      struct point_t *p[3];
      for(j = 0; j < 3; j++) {
	 put_pointdata[type](&data, points[j], format);
         if(format == METEOR_INDEX) {
            /* we are given the index of an existing point */
            p[j] = Heap[points[j]->index];
            if(points[j]->index < 0 || points[j]->index >= PointCount)
               ERROR("Index out of range");
         } else {
            /* we are given point data, so try to find an existing point with the
               same data, if it cannot be found, create a new point */
            int m;
            for(m = 0; m < PointCount; m++) {
               struct point_t *q = Heap[m];
               int k, l;
               for(k = 0; k < 3; k++) {
                  if(q->pos[k] != points[j]->pos[k])
                     goto nextpoint;
                  for(l = 0; l < DataParts; l++)
                     if(q->data[l*3 + k] != points[j]->data[l*3 + k])
                        goto nextpoint;
               }
	       p[j] = q;
	       goto foundit;

            nextpoint:;
            }
            /* couldn't find a point to match up, create it */
            p[j] = NewPoint();
            memcpy(p[j], points[j], sizeof points[j]);
         foundit:;
         }
      }

      /* finally have internal pointers to the points in this triangle */
      NewTriangle(p[0], p[1], p[2]);
      MeshModified = 1;
   }
   return i;
}

const char *meteorError(void)
{
   if(newmeteorerror) {
      newmeteorerror = 0;
      return meteorerror;
   }
   return NULL;
}
