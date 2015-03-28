/*
 * Written in 2007 by Sean D'Epagnier
 *
 * This software is placed in the public domain and can be used freely
 * for any purpose. It comes without any kind of warranty, either
 * expressed or implied, including, but not limited to the implied
 * warranties of merchantability or fitness for a particular purpose.
 * Use it at your own risk. the author is not responsible for any damage
 * or consequences raised by use or inability to use this program.
 */

/* This demo demonstrates how to use libmeteor to generate triangle data from
   an equation, and display it using opengl, a sphere will be displayed */

#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <meteor.h>

static double sphere(double x, double y, double z)
{
   return x*x + y*y + z*z - .5;
}

static void normal(double x[3], double y[3])
{
   x[0] = y[0];
   x[1] = y[1];
   x[2] = y[2];
}

static void display(void)
{
   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   meteorRewind();
   double data[18];

   glBegin(GL_TRIANGLES);
   while(meteorReadTriangles(1, METEOR_COORDS | METEOR_NORMALS,
                           METEOR_DOUBLE, data) == 1) {
      glNormal3dv(data + 3);
      glVertex3dv(data + 0);
      glNormal3dv(data + 9);
      glVertex3dv(data + 6);
      glNormal3dv(data + 15);
      glVertex3dv(data + 12);
   }
   glEnd();

   glFlush ();
}

static void init (void)
{
   glClearColor(0, 0, 0, 0);
   glLoadIdentity();
   glTranslated(0, 0, -2);

   /* lighting */
   GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
   GLfloat mat_shininess[] = { 4.0 };
   GLfloat light_position[] = { 1.0, 1.0, 1.5, 0.0 };

   glShadeModel (GL_SMOOTH);

   glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
   glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
   glLightfv(GL_LIGHT0, GL_POSITION, light_position);

   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_DEPTH_TEST);

   /* set up and build the meteor */
   meteorSetSize(-1, 1, -1, 1, -1, 1, .1);
   meteorReset(METEOR_COORDS | METEOR_NORMALS);
   meteorFunc(sphere);
   meteorNormalFunc(normal);
   while(meteorBuild());
}

static void keyboard(unsigned char k, int x, int y)
{
   exit(0);
}

static void reshape(int w, int h)
{
   glViewport(0, 0, w, h);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(60, (double)w/(double)h, 1, 20);
   glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv)
{
   glutInit(&argc, argv);
   glutInitDisplayMode (GLUT_SINGLE | GLUT_RGB | GLUT_DEPTH);
   glutCreateWindow ("glut demo");

   init();
   glutReshapeFunc(reshape);
   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   glutMainLoop();
   return 0;
}
