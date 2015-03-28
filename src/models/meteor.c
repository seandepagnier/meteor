#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>

double func(double x, double y, double z)
{
   return (x*z + x*x + y*y + z*z) - .2 - ((double)rand()/(double)RAND_MAX)*.1;
}
#if 0
void normal(double x[3], double y[3])
{
   x[0] = y[0];
   x[1] = y[1];
   x[2] = y[2];
}
#endif

void color(double c[3], double p[3])
{
   c[0] = 1;
   c[1] = 1;
   c[2] = 1;
}
