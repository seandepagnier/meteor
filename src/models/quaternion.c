#include <math.h>
#include <GL/gl.h>

static double R = .7;
static double P;

void update(void)
{
   static double x, y, z;
   x += .5;
   y += .5;
   z += .5;
   glRotated(2, cos(x/40)/3,sin(y/20),sin(z/30));

   R+=.01;
   P=sin(R)/10;
}

double func(double x, double y, double z)
{
   double xp = P, yp = P-.3, zp = z+.4, wp = R;
   double w = R;
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
#if 0
void texcoord(double t[3], double p[3])
{
   t[0] = p[0];
   t[1] = p[1];
   t[2] = p[2];
}
#endif
#if 0
void color(double t[3], double p[3])
{
   t[0] = p[0];
   t[1] = p[1];
   t[2] = p[2];
}
#endif

