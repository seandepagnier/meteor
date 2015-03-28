/*
 * Copyright (C) 2007  Sean D'Epagnier   All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>

/* this program is used to generate the code in addCubeTetras (build.c)
void printtetra(int p1, int p2, int p3, int v1, int v2, int v3, int v4, int v5, int v6)
{
   static const int jmptable[8] =
      {1, 5, 6, 4, 7, 2, 3, 0};

#define T(a, b, c) printf("T(%d, %d, %d) ", v##a, v##b, v##c);

   switch(jmptable[(!p1*2+!p2)*2+!p3]) {
   case 1: T(1, 2, 3) break;
   case 2: T(2, 5, 4) break;
   case 3: T(3, 6, 5) break;
   case 4: T(1, 4, 6) break;
   case 5: T(1, 2, 6) T(2, 5, 6) break;
   case 6: T(1, 4, 3) T(3, 4, 5) break;
   case 7: T(2, 3, 4) T(3, 6, 4) break;
   }
}

int main(void)
{
   int p4, p2, p1, p8, p14, p15, p18;
   for(p4 = 1; p4 >= 0; p4--)
      for(p2 = 1; p2 >= 0; p2--)
         for(p1 = 1; p1 >= 0; p1--)
            for(p8 = 1; p8 >= 0; p8--)
               for(p14 = 1; p14 >= 0; p14--)
                  for(p15 = 1; p15 >= 0; p15--)
                     for(p18 = 1; p18 >= 0; p18--) {
                        printf("case %d: ",
                               (((((!p4*2+!p2)*2+!p1)*2+!p8)*2+!p14)*2+!p15)*2+!p18);
                        printtetra(p18, p15, p4, 18, 15, 4, 12, 16, 17);
                        printtetra(p18, p4, p2, 18, 4, 2, 17, 3, 7);
                        printtetra(p18, p2, p1, 18, 2, 1, 7, 0, 6);
                        printtetra(p18, p1, p8, 18, 1, 8, 6, 5, 9);
                        printtetra(p18, p8, p14, 18, 8, 14, 9, 10, 11);
                        printtetra(p18, p14, p15, 18, 14, 15, 11, 13, 12);
                        printf("break;\n");
                     }
}
