#include <GL/gl.h>
#include <math.h>

const double R = .25, r = .16;

double func(double x, double y, double z)
{
   double c = R - sqrt(x*x + y*y);
   return c*c + z*z*.75 - r*r;
}

void texcoord(double t[3], double p[3])
{
   t[0] = p[0]+.5;
   t[1] = p[1]+.5;
   t[2] = p[2]+.5;
}

void update(void)
{
   glRotated(5, 0, 0, -1);
}
