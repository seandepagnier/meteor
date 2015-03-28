double func(double x, double y, double z)
{
   if(x > 0)
      x -= .32;
   else
      x += .32;
   return (x*x + y*y + z*z) - .1;
}
