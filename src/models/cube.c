#include <math.h>

static const double rad = .5;
static double face[6];
static int minface;

static inline void getfaces(double x, double y, double z)
{
   face[0] = rad + x, face[1] = rad - x;
   face[2] = rad + y, face[3] = rad - y;
   face[4] = rad + z, face[5] = rad - z;
   double min = face[0];
   minface = 0;
   int i;
   for(i = 1; i<6; i++)
      if(face[i] < min) {
         min = face[i];
         minface = i;
      }
}

double func(double x, double y, double z)
{
   getfaces(x, y, z);
   return -face[minface];
}

#if 0
void normal(double n[3], double p[3])
{
   getfaces(p[0], p[1], p[2]);
   switch(minface) {
   case 0: n[0] = -1, n[1] =  0, n[2] =  0; break;
   case 1: n[0] =  1, n[1] =  0, n[2] =  0; break;
   case 2: n[0] =  0, n[1] = -1, n[2] =  0; break;
   case 3: n[0] =  0, n[1] =  1, n[2] =  0; break;
   case 4: n[0] =  0, n[1] =  0, n[2] = -1; break;
   case 5: n[0] =  0, n[1] =  0, n[2] =  1; break;
   }
}
#endif

#if 0
void color(double c[3], double p[3])
{
   getfaces(p[0], p[1], p[2]);
   switch(minface) {
   case 0: c[0] =  1, c[1] =  0, c[2] =  0; break;
   case 1: c[0] =  1, c[1] =  1, c[2] =  0; break;
   case 2: c[0] =  0, c[1] =  1, c[2] =  0; break;
   case 3: c[0] =  0, c[1] =  1, c[2] =  1; break;
   case 4: c[0] =  0, c[1] =  0, c[2] =  1; break;
   case 5: c[0] =  1, c[1] =  0, c[2] =  1; break;
   }
}
#endif
