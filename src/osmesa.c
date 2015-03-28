/*
 * Copyright (C) 2007  Sean d'Epagnier   All Rights Reserved.
 *
 * Meteor is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Meteor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_LIBOSMESA

#include <signal.h>
#include <setjmp.h>

#include <GL/osmesa.h>

#include "util.h"
#include "opengl.h"
#include "video.h"
#include "config.h"

static sigjmp_buf osmesajmpbuf;
static RETSIGTYPE siginthandler(int sig)
{
   siglongjmp(osmesajmpbuf, 1);
}

static void draw(void)
{
   double time = getdtime();
   verbose_printf("drawing... ");
   display();
   verbose_printf("%g seconds\n", getdtime() - time);   
}

void osmesaMainLoop(char *filename)
{
   OSMesaContext ctx;
   if(!(ctx = OSMesaCreateContext(OSMESA_RGB, NULL)))
      die("OSMesaCreateContext failed!\n");
   if(!defaultWidth)
      defaultWidth = 640, defaultHeight = 480;
   
   unsigned char *displaybuffer;
   if(!(displaybuffer = malloc(defaultWidth * defaultHeight * 3)))
      die("malloc failed\n");
   /* Bind the buffer to the context and make it current */
   if (!OSMesaMakeCurrent(ctx, displaybuffer, GL_UNSIGNED_BYTE,
                             defaultWidth, defaultHeight ))
      die("OSMesaMakeCurrent failed!\n");
   
   reshape(defaultWidth, defaultHeight);
   init();
   
   if(animated) {
      int vp[4];
      glGetIntegerv(GL_VIEWPORT, vp);
      
      if(videoStart(filename, vp[2], vp[3]) == -1)
         exit(-1);

      int frame = 0;
      signal(SIGINT, siginthandler);
      if(!sigsetjmp(osmesajmpbuf, 0))
         while(!animationdone) {
            draw();
            WriteVideoFrame();
            verbose_printf("wrote frame: %d\r", frame++);
            fflush(stdout);
            update();
         }
      verbose_printf("\n");
      videoStop();
   } else {
      draw();
      TakeScreenShot(filename);
   }

   OSMesaDestroyContext(ctx);
   free(displaybuffer);
   exit(0);
}

#else

void osmesaMainLoop(const char *filename)
{
   die("Would generate %s, but compiled without osmesa support\n", filename);
}

#endif
