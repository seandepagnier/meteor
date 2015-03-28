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

#include <limits.h>

void init(void);
void display(void);
void setProjection(void);
void reshape(int w, int h);
void keypress(unsigned char key);
void WriteVideoFrame(void);
void TakeScreenShot(char *filename);

int openglParseArgs(int c);

extern char texturefilename[PATH_MAX];

extern unsigned int defaultWidth, defaultHeight;
extern int videoenabled, screenshot;

extern double Trans[3];
extern double Fov, Near, Far;

extern int MeshFlags;

extern int bail;

int update(void);

extern int animated, animationdone;
extern int normals;
