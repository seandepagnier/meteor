#include <math.h>

double func(double x, double y, double z)
{
   return -y;
}


void texture(double t[2], double p[3])
{
   t[0] = -p[0]+.3;
   t[1] = -p[2]/2+.04+p[0]/4;

   int i;
   for(i=0; i<3; i++) {
      while(t[i] < 0)
         t[i] += 1;
      while(t[i] >= 1)
         t[i] -= 1;
   }

}
