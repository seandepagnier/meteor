double func(double x, double y, double z)
{
   const double c = sqrt(sqrt(.2001));
   double x1 = x - c, x2 = x + c;
   return x1*x1*x2*x2 + z*z + y*y - .2;
}
