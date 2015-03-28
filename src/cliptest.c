double func(double x, double y, double z)
{
   return x*x + y*y + z*z - .8;
}

static double sub(double x)
{
   return (x-.5) * (x-.25) * x * (x+.25) * (x+.5);
}

double clip(double x, double y, double z)
{
   return sub(x) * sub(y) * sub(z);
}
