#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>

static double R = .1;

double func(double x, double y, double z)
{
   return x*x*x*x + y*y*y*y + z*z*z*z - R;
}
#if 0
void texture(double c[3], double p[3])
{
   c[0] = fabs(p[0]);
   c[1] = fabs(p[1]);
   c[2] = fabs(p[2]);
}
#endif 
#if 0
void color(double c[3], double p[3])
{
   c[0] = fabs(p[0]);
   c[1] = fabs(p[1]);
   c[2] = fabs(p[2]);
}
#endif
void update(void)
{
   R+=.02;
}
