#include <math.h>

double func(double x, double y, double z)
{
   return x*x + y*y + z*z - .8;
}

void texcoord(double t[3], double p[3])
{
   t[0] = (atan2(p[0], p[1])+M_PI)/(2*M_PI);
   t[1] = acos(p[2]/sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2])) / M_PI;
   t[2] = 0;
}
