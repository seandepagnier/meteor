#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>

static double R = .1;

void init(void)
{
}

double func(double x, double y, double z)
{
   return (x*x + y*y + z*z) - .2;
}
#if 0
void texcoord(double c[3], double p[3])
{
   c[0] = fabs(p[0]);
   c[1] = fabs(p[1]);
   c[2] = fabs(p[2]);
}
#endif 
#if 1
void color(double c[3], double p[3])
{
   c[0] = fabs(p[0]) + .4;
   c[1] = fabs(p[1])*R + .4;
   c[2] = fabs(p[2])*2 + .4;
}
#endif
void update(void)
{
   R+=.1;
}
