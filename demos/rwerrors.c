/*
 * Written in 2007 by Sean D'Epagnier
 *
 * This software is placed in the public domain and can be used freely
 * for any purpose. It comes without any kind of warranty, either
 * expressed or implied, including, but not limited to the implied
 * warranties of merchantability or fitness for a particular purpose.
 * Use it at your own risk. the author is not responsible for any damage
 * or consequences raised by use or inability to use this program.
 */

/* the purpose of this program is to demonstrate errors caused by
   incorrectly working with meteor data */

#include <stdio.h>
#include <assert.h>
#include <meteor.h>

static double sphere(double x, double y, double z)
{
   return x*x + y*y + z*z - .5;
}

int main(void)
{
   double d[9];
   float f[9];
   int i[3];
   int ret;

   /* generate data from -1 to 1 for x,y,z with .3 step size */
   meteorSetSize(-1, 1, -1, 1, -1, 1, .3);
   meteorFunc(sphere);
   while(meteorBuild());

   /* this correctly calls meteorReadPoints */
   meteorRewind();
   ret = meteorReadPoints(1, METEOR_COORDS, METEOR_DOUBLE, d);
   assert(ret == 1);

   /* this meteor does not have normal data, try to read it */
   meteorRewind();
   ret = meteorReadPoints(1, METEOR_NORMALS, METEOR_DOUBLE, d);
   assert(ret == -1);
   printf("Expected Error: %s\n", meteorError());

   /* try reading data in as floats */
   meteorRewind();
   ret = meteorReadPoints(1, METEOR_COORDS, METEOR_FLOAT, f);

   /* merge the meteor */
   meteorRewind();
   ret = meteorMerge();
   assert(ret != 0);

   /* now the meteor is modified, reading points will fail without a rewind */
   ret = meteorReadPoints(1, METEOR_COORDS, METEOR_DOUBLE, d);
   assert(ret == -1);
   printf("Expected Error: %s\n", meteorError());

   /* again, reading points should work */
   meteorRewind();
   ret = meteorReadPoints(1, METEOR_COORDS, METEOR_DOUBLE, d);
   assert(ret == 1);

   /* try reading triangles */
   meteorRewind();
   ret = meteorReadTriangles(1, METEOR_INDEX, METEOR_INT, i);
   assert(ret == 1);

   /* lets see how many points and triangles we have now */
   printf("%d points, %d triangles\n", meteorPointCount(), meteorTriangleCount());

   /* write some indexes that are out of range */
   i[0] = 1000;
   ret = meteorWriteTriangles(1, METEOR_INDEX, METEOR_INT, i);
   assert(ret == -1);
   printf("Expected Error: %s\n", meteorError());

   /* write some valid indexes, this wipes any triangles that existed before */
   i[0] = 1;
   ret = meteorWriteTriangles(1, METEOR_INDEX, METEOR_INT, i);
   assert(ret == 1);

   /* should have 1 more triangle now */
   printf("%d points, %d triangles\n", meteorPointCount(), meteorTriangleCount());

   /* try writing points */
   ret = meteorWritePoints(1, METEOR_COORDS, METEOR_DOUBLE, d);
   assert(ret == 1);

   /* try writing triangles */
   ret = meteorWriteTriangles(1, METEOR_COORDS, METEOR_DOUBLE, d);
   assert(ret == 1);

   /* should have 3 points and 1 triangle, the last triangle should reused the
      point written since the coordinates are the same */
   printf("%d points, %d triangles\n", meteorPointCount(), meteorTriangleCount());

   printf("Success!\n");
   return 0;
}
