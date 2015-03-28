static double r = .5;

double func(double x, double y, double z)
{
   return (x*x + y*y + z*z) - r*r;
}

void update(void)
{
   r += .01;
}
