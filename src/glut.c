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

#include "config.h"

#ifdef HAVE_LIBGLUT

#include <GL/glut.h>

#include "util.h"
#include "opengl.h"

enum {LEFT, RIGHT, UP, DOWN, INSERT, DELETE, PAGEUP, PAGEDOWN,
      STRAFEL, STRAFER, STRAFEU, STRAFED, ONE, TWO, THREE, FOUR,
      FIVE, SIX};
#define NUM_KEYS SIX + 1

static const double speed = 3, maxTransdt = 1, sspeed = .03, pspeed = 1.1;
static int Modifiers;

static int Keys;

static double Transdt[3];

/* for fps calculation */
static int frames;

static void fps()
{
   verbose_printf("fps %d  \r", frames);
   frames = 0;
   glutTimerFunc(1000, fps, 0);
}

/* position, speed and current inputs */
static void RotateAfter(double ang, double x, double y, double z)
{
   double m[16];
   glGetDoublev(GL_MODELVIEW_MATRIX, m);
   glLoadIdentity();
   glRotated(ang, x, y, z);
   glMultMatrixd(m);
}

static void idle(void)
{
   int i;

   double cspeed = speed;

   if(Modifiers & GLUT_ACTIVE_CTRL)
      cspeed *= 2;
   if(Modifiers & GLUT_ACTIVE_SHIFT)
      cspeed /= 2;

   for(i = 0; i<NUM_KEYS; i++) {
      if(Keys & (1 << i)) {
	 switch(i) {
	 case LEFT:        RotateAfter(cspeed, 0, 1, 0);   break;
	 case RIGHT:       RotateAfter(-cspeed, 0, 1, 0);  break;
	 case UP:          RotateAfter(cspeed, 1, 0, 0);   break;
	 case DOWN:        RotateAfter(-cspeed, 1, 0, 0);  break;
	 case INSERT:      RotateAfter(cspeed, 0, 0, 1);   break;
	 case DELETE:      RotateAfter(-cspeed, 0, 0, 1);  break;
	 case STRAFEL:     Transdt[0] += sspeed;     break;
	 case STRAFER:     Transdt[0] -= sspeed;     break;
	 case STRAFEU:     Transdt[1] -= sspeed;     break;
	 case STRAFED:     Transdt[1] += sspeed;     break;
	 case PAGEUP:      Transdt[2] += sspeed;     break;
	 case PAGEDOWN:    Transdt[2] -= sspeed;     break;
         case ONE:      Far /= pspeed;  setProjection(); break;       
         case TWO:      Far *= pspeed;  setProjection(); break;
         case THREE:    Near /= pspeed; setProjection(); break;       
         case FOUR:     Near *= pspeed; setProjection(); break;
         case FIVE:     Fov /= pspeed;  setProjection(); break;       
         case SIX:      Fov *= pspeed;  setProjection(); break;
	 }
         glutPostRedisplay();
      }
   }

   if(!(Keys & (1 << STRAFEL)) && !(Keys & (1 << STRAFER)))
      Transdt[0] = 0;
   if(!(Keys & (1 << STRAFEU)) && !(Keys & (1 << STRAFED)))
      Transdt[1] = 0;
   if(!(Keys & (1 << PAGEUP)) && !(Keys & (1 << PAGEDOWN)))
      Transdt[2] = 0;

   for(i = 0; i < 3; i++) {
      if(Transdt[i] > maxTransdt)
	 Transdt[i] = maxTransdt;

      Trans[i] += Transdt[i];
   }

   if(Modifiers & GLUT_ACTIVE_SHIFT)
      Transdt[0] = Transdt[1] = Transdt[2] = 0;
}

static void hit(int code, int up)
{
   if(up)
      Keys &= ~(1 << code);
   else
      Keys |= 1 << code;
}

static void keyboard(unsigned char key, int up)
{
   Modifiers = glutGetModifiers();
   key = tolower(key);
   switch(key) {
   case '\b': hit(DELETE, up); break;
   case 'a': hit(STRAFEL, up); break;
   case 'd': hit(STRAFER, up); break;
   case 'w': hit(STRAFEU, up); break;
   case 's': hit(STRAFED, up); break;
   case '1': hit(ONE, up);     break;
   case '2': hit(TWO, up);     break;
   case '3': hit(THREE, up);   break;
   case '4': hit(FOUR, up);    break;
   case '5': hit(FIVE, up);    break;
   case '6': hit(SIX, up);     break;
   }
}

static void keyboarddown(unsigned char key, int x, int y)
{
   x = y = 0;
   keyboard(key, 0);
   keypress(key);
   glutPostRedisplay();
}

static void keyboardup(unsigned char key, int x, int y)
{
   x = y = 0;
   keyboard(key, 1);
   glutPostRedisplay();
}

static void special(int key, int up)
{
   Modifiers = glutGetModifiers();
   switch(key) {
   case GLUT_KEY_LEFT:      hit(LEFT, up);     break;
   case GLUT_KEY_RIGHT:     hit(RIGHT, up);    break;
   case GLUT_KEY_UP:        hit(UP, up);       break;
   case GLUT_KEY_DOWN:      hit(DOWN, up);     break;
   case GLUT_KEY_INSERT:    hit(INSERT, up);   break;
   case GLUT_KEY_PAGE_UP:   hit(PAGEUP, up);   break;
   case GLUT_KEY_PAGE_DOWN: hit(PAGEDOWN, up); break;
   }
   glutPostRedisplay();
}

static void specialdown(int key, int x, int y)
{
   x = y = 0;
   special(key, 0);
}

static void specialup(int key, int x, int y)
{
   x = y = 0;
   special(key, 1);
}

static int lastx, lasty, lastbutton;
static void mousefunc(int button, int state, int x, int y)
{
   if(state == GLUT_DOWN) {
      lastbutton = button;
      lastx = x;
      lasty = y;
   }
}

static void motionfunc(int x, int y) {
   int dx = x - lastx, dy = y - lasty;
   switch(lastbutton) {
   case GLUT_LEFT_BUTTON:
      RotateAfter(dx/4, 0, 1, 0);
      RotateAfter(dy/4, 1, 0, 0);
      break;
   case GLUT_MIDDLE_BUTTON:
      Trans[0] += (double)dx/100.0;
      Trans[1] -= (double)dy/100.0;
      break;
   case GLUT_RIGHT_BUTTON:
      RotateAfter(dx/4, 0, 0, 1);
      Trans[2] -= (double)dy/20.0;
      break;
   }
   lastx = x, lasty = y;
   glutPostRedisplay();
}

static void glutdisplay(void)
{
   display();

   if(videoenabled)
      WriteVideoFrame();

   if(screenshot) {
      static int scrnum;
      char filename[32];
      sprintf(filename, "scrnshot%d.png", scrnum++);
      TakeScreenShot(filename);

      screenshot = 0;
   }

   glutSwapBuffers();

   frames++;

   if(update())
      glutPostRedisplay();
}

void startglut(int *argc, char **argv)
{
   glutInit(argc, argv);
   glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
   if(defaultWidth)
      glutInitWindowSize(defaultWidth, defaultHeight);
   glutCreateWindow(argv[0]);

   glutReshapeFunc(reshape);
   glutDisplayFunc(glutdisplay);
   glutKeyboardFunc(keyboarddown);
   glutKeyboardUpFunc(keyboardup);
   glutSpecialFunc(specialdown);
   glutSpecialUpFunc(specialup);
   glutMouseFunc(mousefunc);
   glutMotionFunc(motionfunc);
   glutIdleFunc(idle);

   /* uncomment to display fps information */
      // glutTimerFunc(1000, fps, 0);

   init();
   glutMainLoop();
}

#else

void startglut(int *argc, char **argv)
{
   die("Would display, but compiled without glut support, exiting.\n");
}

#endif
