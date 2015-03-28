#include <math.h>

double func(double x, double y, double z)
{
   return x*x + y*y + z*z - .5;
}

void texcoord(double t[3], double p[3])
{
   t[0] = (p[0]+1)/2;
   t[1] = (p[1]+1)/2;
   t[2] = (p[2]+1)/2;
}
