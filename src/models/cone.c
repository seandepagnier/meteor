#include <math.h>

double func(double x, double y, double z)
{
   return (x*x + z*z) - y*y;
}
#if 0
void color(double c[3], double p[3])
{
   c[0] = fabs(p[0]) + .4;
   c[1] = fabs(p[1])*R + .4;
   c[2] = fabs(p[2])*2 + .4;
}
#endif
