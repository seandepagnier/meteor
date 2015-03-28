#include <math.h>

double func(double x, double y, double z)
{
   double xp = x, yp = y-.3, zp = z+.4, wp = 1;
   double w = x;
   int i;
   double i2;

   x*=1.5;
   y*=1.5;
   z*=1.5;

   for(i = 0; i<7; i++) {
      i2 = y*y + z*z + w*w;
      y = 2*x*y + yp;
      z = 2*x*z + zp;
      w = 2*x*w + wp;
      x = x*x - i2 + xp;
   }
   return x*x + i2 - 2;
}
