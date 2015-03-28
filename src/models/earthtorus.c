#include <GL/gl.h>
#include <math.h>

const double R = .4, r = .25;

void init(void)
{
}

double func(double x, double y, double z)
{
   double c = R - sqrt(x*x + y*y);
   return c*c + z*z*.75 - r*r;
}

void texcoord(double t[3], double p[3])
{
   double ang = atan2(p[1], p[0]);
   double nx = R*cos(ang)*2.5;
   double dist = p[0]/nx - R;
   double ang2 = atan2(p[2], dist);

   t[0] = (ang+M_PI)/(2*M_PI);
   t[1] = (ang2+M_PI)/(2*M_PI);
   t[2] = 0;
}

void update(void)
{
   glRotated(5, 0, 0, -1);
}
