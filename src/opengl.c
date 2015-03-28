/*
 * Copyright (C) 2007  Sean D'Epagnier   All Rights Reserved.
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

#include <math.h>

#include "config.h"
#include "opengl.h"

char texturefilename[PATH_MAX] = "";

unsigned int defaultWidth = 0, defaultHeight = 0;
int videoenabled = 0, screenshot = 0;

double Trans[3] = {0, 0, -2.3};
double Fov = 50, Near = .1, Far = 20;

int MeshFlags;

int bail = 0;

#if defined(HAVE_LIBGL) && defined(HAVE_LIBGLU)

#include <getopt.h>
#include <string.h>

#include <math.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "image.h"
#include "video.h"
#include "util.h"
#include "glut.h"
#include "osmesa.h"
#include "opengl.h"

#include "meteor.h"

static char initialkeys[256];
static int tex3D;
static int rebuild = 1;

static int usevbos;

static inline void sub3f(float x[3], float a[3], float b[3])
{
   x[0] = a[0] - b[0];
   x[1] = a[1] - b[1];
   x[2] = a[2] - b[2];
}

static inline void crossf(float x[3], float a[3], float b[3])
{
   x[0] = a[1] * b[2] - a[2] * b[1];
   x[1] = a[2] * b[0] - a[0] * b[2];
   x[2] = a[0] * b[1] - a[1] * b[0];
}

static void buildTexture(int type)
{
   glEnable(type);
   glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   Image *image;
   if(!(image = image_load_png(texturefilename)))
      die("failed to load %s\n", texturefilename);

   if(type == GL_TEXTURE_2D) {
      glTexImage2D(type, 0, GL_RGBA, image->width, image->height,
                   0, GL_RGB, GL_UNSIGNED_BYTE, image->data);
   } else {
      const int width = 128;
      int xi, yi, zi;
      unsigned char *data = malloc(width*width*width*3);
      
      for(xi=0; xi<width; xi++)
         for(yi=0; yi<width; yi++)
            for(zi=0; zi<width; zi++) {
               int off = ((xi*width+yi)*width+zi)*3;
               double x = (double)xi/(double)width-.5;
               double y = (double)yi/(double)width-.5;
               double z = (double)zi/(double)width-.5;
               double ang = atan2(x, z);
               double len = sqrt(x*x + y*y + z*z);
               if(len == 0) len += .0001;
               int fromx = (ang+M_PI)/(2*M_PI)*(double)(image->width-1);
               int fromy = acos(y/len) / M_PI * (double)(image->height-1);
               int fromoff = (fromy*image->width+fromx)*3;
            
               data[off+0] = image->data[fromoff+0];
               data[off+1] = image->data[fromoff+1];
               data[off+2] = image->data[fromoff+2];
            }
      
      glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, width, width, width, 0,
                   GL_RGB, GL_UNSIGNED_BYTE, data);
      free(data);
   }
   
   free(image->data);
   free(image);
}

void keypress(unsigned char key)
{
   static int c, w, f, one, i;
   switch(key) {
   case 'm':
      if(!meteorMerge())
         warning("cannot reduce meteor further\n");
      rebuild = 1;
      break;
   case 'o':
      one = !one;
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, one);
      break;
   case 27:
   case 'q':
      if(videoenabled)
         videoStop();
      exit(0);
   case 'v':
      if(!videoenabled) {
         int vp[4];
         glGetIntegerv(GL_VIEWPORT, vp);

         if(videoStart("video.mp4", vp[2], vp[3]) == 0)
            videoenabled = 1;
      } break;
   case 'b':
      if(videoenabled)
         videoStop();
      videoenabled = 0;
      break;
   case 'n':
      screenshot = 1;
      break;
   case 'c':
      if(c = !c)
         glDisable(GL_CULL_FACE);
      else
         glEnable(GL_CULL_FACE);
      break;
   case 'l':
      glPolygonMode(GL_FRONT_AND_BACK, (w = !w) ? GL_LINE: GL_FILL);
      break;
   case 'f':
      glShadeModel((f = !f) ? GL_FLAT : GL_SMOOTH);
      break;
   }
}

/* function for determining if extensions are supported or not */
static int isglExtensionSupported(const char *extension)
{
   const char *exts = (const char *) glGetString(GL_EXTENSIONS);
   const char *start = exts;

   int len = strlen(extension);

   if(!exts)
      return 0;

   for(;;) {
      const char *p = strstr(exts, extension);
      if(!p)
	 break;
      if((p == start || p[-1] == ' ') && (p[len] == ' ' || p[len] == 0))
	 return 1;
      exts = p + len;
   }
   return 0;
}

/* display and transformations */
static void initlights(void)
{
   GLfloat ambient[] = {0, 0, 0, 1.0};
   GLfloat mat_diffuse[] = {0.6, 0.6, 0.6, 1.0};
   GLfloat mat_specular[] = {.5, .5, .5, 0};
   GLfloat mat_shininess[] = {32.0};

   glEnable(GL_COLOR_MATERIAL);
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);

   glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

   /*
     GLfloat att;
     att = 0;
     glLightfv(GL_LIGHT0, GL_CONSTANT_ATTENUATION, &att);
     att = 0;
     glLightfv(GL_LIGHT0, GL_LINEAR_ATTENUATION, &att);
     att = .3;
     glLightfv(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, &att);
   */

   glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

/* initialize opengl state */
void init(void)
{
   glClearColor(0.0, 0.0, 0.0, 1.0);

   glEnable(GL_NORMALIZE);
   glEnable(GL_DEPTH_TEST);

   if(normals)
      initlights();

   if(texturefilename[0]) {
      if(tex3D)
         buildTexture(GL_TEXTURE_3D);
      else
         buildTexture(GL_TEXTURE_2D);
   }

   glEnable(GL_CULL_FACE);

   unsigned int i;
   for(i = 0; i < sizeof initialkeys; i++)
      if(initialkeys[i])
         if(i == 'q')
            bail = 1;
         else
            keypress(i);

   if(!animated && isglExtensionSupported("GL_ARB_vertex_buffer_object")) {
      usevbos = 1;

      GLuint buffers[2];
      glGenBuffersARB(2, buffers);
      glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers[0]);
      glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers[1]);
   }

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}

static int parts(int format)
{
   if(!format)
      return 0;
   int next = parts(format >> 1);
   if(format & 1)
      next++;
   return next;
}

static int trianglenum;
static unsigned int *triangledata;

static void rebuildtriangles(void)
{
   int format = meteorFormat();

   static float *pdata;

   int psize, tsize, len = parts(format)*3;
   int stride = len * sizeof(*pdata);
   int pn = meteorPointCount();
   trianglenum = meteorTriangleCount();

   if(!pn || !trianglenum)
      return;

   psize = pn * stride;
   if(!(pdata = realloc(pdata, psize)))
      die("failed to allocate a buffer of %d bytes\n", psize);

   tsize = trianglenum * 3 * sizeof(*triangledata);
   if(!(triangledata = realloc(triangledata, tsize)))
      die("failed to allocate a buffer of %d bytes\n", tsize);

   meteorRewind();
   if(meteorReadPoints(pn, format, METEOR_FLOAT, pdata) != pn)
      die("failed to read point data for %d points\n", pn);

   if(meteorReadTriangles(trianglenum, METEOR_INDEX,
                            METEOR_UNSIGNED_INT, triangledata) != trianglenum)
      die("failed to read triangle data for %d triangles\n", trianglenum);

   if(usevbos) {
      glBufferDataARB(GL_ARRAY_BUFFER_ARB, pn * stride, pdata, GL_STATIC_DRAW_ARB);
      free(pdata);
      pdata = 0;
   }

   float *poff = pdata;

   glVertexPointer(3, GL_FLOAT, stride, poff);
   glEnableClientState(GL_VERTEX_ARRAY);
   poff += 3;

   if(format & METEOR_NORMALS) {
      glNormalPointer(GL_FLOAT, stride, poff);
      glEnableClientState(GL_NORMAL_ARRAY);
      poff += 3;
   }

   if(format & METEOR_COLORS) {
      glColorPointer(3, GL_FLOAT, stride, poff);
      glEnableClientState(GL_COLOR_ARRAY);
      poff += 3;
   }

   if(format & METEOR_TEXCOORDS) {
      glTexCoordPointer(tex3D ? 3 : 2, GL_FLOAT, stride, poff);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      poff += 3;
   }

   if(usevbos) {
      glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 3*trianglenum*sizeof(*triangledata),
                      triangledata, GL_STATIC_DRAW_ARB);
      free(triangledata);
      triangledata = 0;
   }
}

static void drawmeteor(void)
{
   if(animated || rebuild) {
      rebuildtriangles();
      rebuild = 0;
   }

   glDrawElements(GL_TRIANGLES, 3*trianglenum, GL_UNSIGNED_INT, triangledata);
}

static void TranslateAfter(double x, double y, double z)
{
   double m[16];
   glGetDoublev(GL_MODELVIEW_MATRIX, m);
   glLoadIdentity();
   glTranslated(x, y, z);
   glMultMatrixd(m);
}

void display(void)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glLoadIdentity();
   GLfloat position[] = {1.0, 1.0, 1.0, 0.0};
   glLightfv(GL_LIGHT0, GL_POSITION, position);
   glPopMatrix();

   glPushMatrix();
   TranslateAfter(Trans[0], Trans[1], Trans[2]);
   drawmeteor();

   glPopMatrix();
}

void WriteVideoFrame(void)
{
   int vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);
   unsigned char pixels[vp[2] * vp[3] * 3];
   glReadPixels(0, 0, vp[2], vp[3], GL_RGB, GL_UNSIGNED_BYTE, pixels);
      
   videoWriteFrame(pixels, vp[2], vp[3]);
}

void TakeScreenShot(char *filename)
{
   int vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);
   unsigned char pixels[vp[2] * vp[3] * 3];
   glReadPixels(0, 0, vp[2], vp[3], GL_RGB, GL_UNSIGNED_BYTE, pixels);
      
   if(write_png(filename, vp[2], vp[3], pixels))
      verbose_printf("Failed to write image to %s\n", filename);
   else
      verbose_printf("Wrote image to %s\n", filename);
}

/* set the near and far planes in the opengl projection matrix */
static int width, height;
void setProjection(void)
{
   glPushAttrib(GL_TRANSFORM_BIT);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(Fov, (GLfloat)width/(GLfloat)height, Near, Far);
   glPopAttrib();
}

void reshape(int w, int h)
{
   width = w, height = h;
   glViewport(0, 0, w, h);
   setProjection();
   glMatrixMode(GL_MODELVIEW);
}

static void addkeys(void)
{
   unsigned char *c;
   for(c = optarg; *c; c++)
      initialkeys[*c] = 1;
}

static void setGeometry(void)
{
   if(sscanf(optarg, "%ux%u", &defaultWidth, &defaultHeight) != 2)
      die("invalid geometry: %s\n", optarg);
}

int openglParseArgs(int c)
{
   switch(c) {
   case 'k': addkeys(); break;
   case 10: strncpy(texturefilename, optarg, PATH_MAX); break;
   case 11: tex3D = 1;
   case 'g': setGeometry(); break;
   default:
      return 0;
   }
   return 1;
}

#else

int openglParseArgs(int c)
{
   return 0;
}
#endif
