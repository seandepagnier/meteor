#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>

double func(double x, double y, double z)
{
   double c = .4 - sqrt(x*x + y*y);
   return c*c + z*z - .04;
}

void color(double c[3], double p[3])
{
   c[0] = fabs(p[0]) + .4;
   c[1] = fabs(p[1])*1 + .4;
   c[2] = fabs(p[2])*2 + .4;
}

