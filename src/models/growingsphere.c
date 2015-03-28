#include <math.h>

static double R=.4;

void update(void)
{
   R+=.01;
}

double func(double x, double y, double z)
{
   return x*x + y*y + z*z - R;
}
