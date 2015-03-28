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

#include <stdio.h>
#include <meteor.h>

/* function used to generate a meteor, should return < 0 when inside,
   0 on the surface, and 1 outside the meteor. */
static double sphere(double x, double y, double z)
{
   return x*x + y*y + z*z - .5;
}

int main(void)
{
   /* tell meteor the min and max area to scan on each axis,
      and the step size, changing these parameters will result
      in different amounts of triangles generated */

   meteorSetSize(-1, 1, -1, 1, -1, 1, .02);
   /* set the meteor generation function */
   meteorFunc(sphere);
   /* build the meteor */
   while(meteorBuild());

   /* print some info to prove the meteor was built */
   printf("%d points, %d triangles\n", meteorPointCount(), meteorTriangleCount());
   return 0;
}
