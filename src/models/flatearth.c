#include <math.h>

double func(double x, double y, double z)
{
   return z+.2;
}

void texcoord(double t[3], double p[3])
{
   double x = -p[0]/2;
   if(x < 0)
      x++;
   double y = -p[1]/2;
   if(y < 0)
      y++;
   t[0] = x;
   t[1] = y;
   t[2] = 0;
}
