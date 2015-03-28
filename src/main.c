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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define _GNU_SOURCE
#include <getopt.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "config.h"
#include "util.h"
#include "opengl.h"

#include "meteor.h"

#ifdef HAVE_LIBLTDL
#include <ltdl.h>
#endif

int animated = 0, animationdone = 0;
static int animationloopmode, animationmaxframes = -1;
static FILE *inputfile, *outputfile;
static int correcttexcoords;
static double Rotation[4], Translation[3], Scale[3] = {1, 1, 1};

/* dl loaded functions */
static void (*updatefunc)(void);
static double (*clipfunc)(double, double, double);

static int num_triangles = -1, max_num_triangles = -1;
static double percent_triangles = -1;

static int propagation;
static double meteoraggregation = -1;

static double (*func)(double, double, double);

static int input_fileformat = -1; /* autodetect */
static int output_fileformat = METEOR_FILE_FORMAT_TEXT;

/* options needed in init */
int normals = 1;

static void defaultnormal(double n[3], double p[3])
{
   const double ns = .000001;
   double ps = func(p[0], p[1], p[2]);
   double np[3] = {};
   n[0] = func(p[0] + ns, p[1], p[2]) - ps;
   np[0] -= ns, np[1] += ns;
   n[1] = func(p[0], p[1] + ns, p[2]) - ps;
   np[1] -= ns, np[2] += ns;
   n[2] = func(p[0], p[1], p[2] + ns) - ps;
}

static void maxTriangles(int cur)
{
   if(max_num_triangles != -1)
      while(meteorTriangleMergeableCount() > max_num_triangles * (cur + 1))
         if(!meteorMerge())
            break;
}

static int builtpoints, builttriangles;

static void build(void)
{
   verbose_printf("building... ");
   
   double time = getdtime();
   int cur, init = meteorBuild();

   if(init == 0)
      goto skiploop;

   struct timeval ltime;
   gettimeofday(&ltime, NULL);

   while((cur = meteorBuild()))
   {
      maxTriangles(0);
      struct timeval ctime;
      gettimeofday(&ctime, NULL);
      int diff =  (ctime.tv_sec - ltime.tv_sec)*1000000 + ctime.tv_usec - ltime.tv_usec;
      if(diff > 100000) { /* update no faster than 10hz */
         ltime = ctime;
         int num = verbose_printf("%.2f%%   ", (double)(init-cur)/init*100.0);
         while(num--)
            verbose_printf("\b");
      }
   }
   maxTriangles(0);

 skiploop:
   verbose_printf("%f seconds\n", getdtime() - time);
   builtpoints = meteorPointCount();
   builttriangles = meteorTriangleCount();
}

static void merge(void)
{
   /* option not specified */
   if(num_triangles == -1)
      if(percent_triangles != -1)
         num_triangles = percent_triangles / 100.0 * meteorTriangleCount();
      else
         return;

   int count = meteorTriangleCount();

   if(count < num_triangles)
      return;

   double time = getdtime();
   int triangles;
   int c, update = (count - num_triangles) / 500 + 1;

   while((triangles=meteorTriangleCount()) > num_triangles) {
      if(c++%update==0 || triangles-1 == num_triangles)
         verbose_printf("merging triangles: %d \r", triangles - 1);
      
      if(!meteorMerge()) {
         warning("failed to merge additional points\n");
         break;
      }
   }

   verbose_printf("merging triangles: %f seconds\n", getdtime() - time);   
}

static void propagate(void)
{
   if(!propagation)
      return;

   if(!normals) {
      warning("propagation is impossible without normals\n");
      propagation = 0;
   }

   verbose_printf("propagating points, %d iterations... ", propagation);
   double time = getdtime();
   double improvement = meteorPropagate(propagation);
   verbose_printf("%f seconds, improvement: %f%%\n",
                  getdtime() - time, 100.0*improvement);
}

static void aggregate(void)
{
   if(meteoraggregation == -1)
      return;

   double time = getdtime();

   int points;
   int i;

   while(points=meteorPointCount() >  meteoraggregation) {
      if(!meteorAggregate()) {
         warning("failed to continue aggregation\n");
         break;
      }
      if(i++ % 10 == 0)
         verbose_printf("aggregating points: %d \r", meteorPointCount());
   }

   verbose_printf("aggregating points: %f seconds\n", getdtime() - time);
}

static void transform(void)
{
   int r = !!Rotation[0];
   int t = !!Translation[0] || !!Translation[1] || !!Translation[2];
   int s = !(Scale[0]==1) || !(Scale[1]==1) || !(Scale[2]==1);
   if(!r && !t && !s)
      return;

   double time = getdtime();
   verbose_printf("applying transformations: ");

   if(r) {
      verbose_printf("rotation ");
      meteorRotate(Rotation[0], Rotation[1], Rotation[2], Rotation[3]);
   }

   if(t) {
      verbose_printf("translation ");
      meteorTranslate(Translation[0], Translation[1], Translation[2]);
   }

   if(s) {
      verbose_printf("scale ");
      meteorScale(Scale[0], Scale[1], Scale[2]);
   }

   verbose_printf("%f seconds\n", getdtime() - time);
}

static void clip(void)
{
   if(!clipfunc)
      return;

   double time = getdtime();
   verbose_printf("clipping... ");
   meteorClip(clipfunc);
   verbose_printf(" %f seconds\n", getdtime() - time);
}

static void correcttexturecoords(void)
{
   if(!correcttexcoords)
      return;

   double time = getdtime();
   verbose_printf("correcting texture coordinates... ");
   meteorCorrectTexCoords();
   verbose_printf(" %f seconds\n", getdtime() - time);
}

static void transformmeteor(void)
{
   merge();
   aggregate();
   propagate();
   clip();
   correcttexturecoords();
   transform();
}

static void info(void)
{
   int points = meteorPointCount(), triangles = meteorTriangleCount();
   int cp = meteorPointCreatedCount(), ct = meteorTriangleCreatedCount();

   if((points != builtpoints || triangles != builttriangles)
      && builtpoints && builttriangles) {
      if(cp != builtpoints || ct != builttriangles)
         verbose_printf("Points: %d/%d/%d\tTriangles: %d/%d/%d\n",
                        points, builtpoints, cp, triangles, builttriangles, ct);
      else
         verbose_printf("Points: %d/%d\tTriangles: %d/%d\n",
                        points, builtpoints, triangles, builttriangles);
   } else
      if(cp != builtpoints || ct != builttriangles)
         verbose_printf("Points: %d/%d\tTriangles: %d/%d\n",
                        points, cp, triangles, ct);
      else
         verbose_printf("Points: %d\tTriangles: %d\n",
                        points, triangles);
}

static struct {
   int format;
   char name[50];
} formattable[] = {{METEOR_FILE_FORMAT_TEXT, "text"},
                   {METEOR_FILE_FORMAT_BINARY, "binary"},
                   {METEOR_FILE_FORMAT_WAVEFRONT, "wavefront"},
                   {METEOR_FILE_FORMAT_VIDEOSCAPE, "videoscape"}};

static const int formattablelen = (sizeof formattable) / (sizeof *formattable);
static void load(void)
{
   double time = getdtime();
   verbose_printf("loading... ");
   if(input_fileformat == -1) {
      verbose_printf("detecting file format... ");
      int i;
      long pos = ftell(inputfile);
      for(i = 0; i < formattablelen; i++) {
         fseek(inputfile, pos, SEEK_SET);
         if(meteorLoad(inputfile, formattable[i].format) == 0) {
            verbose_printf("%s ", formattable[i].name);
            goto loaded;
         }
      }
      die("failed to detect format, try specifying with --input-format\n");
   loaded:;
   } else
      if(meteorLoad(inputfile, input_fileformat) == -1)
         warning("%s\n", meteorError());

   verbose_printf("%f seconds\n", getdtime() - time);   
}

static int save(void)
{
   double time = getdtime();
   verbose_printf("saving... ");
   if(meteorSave(outputfile, output_fileformat) == -1)
      warning("failed: %s\n", meteorError());
   else
      verbose_printf("%f seconds\n", getdtime() - time);   
}

int update(void)
{
#if defined(HAVE_LIBGLUT)
   if(bail)
      keypress('q');
#endif

   static int frames;
   if(++frames == animationmaxframes)
      animationdone = 1;
   /* if it's animated update the meteor */
   if(animated && !animationdone) {
      /* call the update function if there is one */
      if(updatefunc)
         updatefunc();

      if(inputfile) {
         int c = getc(inputfile);
         if(feof(inputfile)) {
            if(animationloopmode)
               rewind(inputfile);
            else {
               animationdone = 1;
               return 0;
            }
         } else
            ungetc(c, inputfile);
      
      load();
      } else
         build();

      transformmeteor();
 
      if(outputfile)
         save();

      info();
      return 1;
   }
   return 0;
}

static int nodisplayloop = 1;
static RETSIGTYPE siginthandler(int sig)
{
   static int counter;
   nodisplayloop = 0;
   if(counter++)
      die("\rinterrupt, aborting\n");
   verbose_printf("\rwill complete current meteor\n");
}

static void nodisplayMainLoop(void)
{
   signal(SIGINT, siginthandler);
   while(nodisplayloop && !animationdone)
      update();
}

#ifdef HAVE_LIBLTDL
void *compileandload(const char *sourcefilename, const char *name)
{
   char filename[] = "/tmp/meteorXXXXXX";
   int fd;
   if((fd = mkstemp(filename) < 0))
      die("mkstemp failed\n");

   double time = getdtime();
   verbose_printf("compiling %s... ", name);
   pid_t pid = fork();

   switch(pid) {
   case -1:
      die("fork failed\n");
   case 0:
      execlp("gcc", "gcc", "-shared", "-O", "-xc", "-fPIC", "-lm", "-o",
             filename, sourcefilename, NULL);
      die("exec failed\n");
   }

   int status;
   wait(&status);
   if(status) {
      unlink(filename);
      die("failed to compile\n");
   }

   verbose_printf("%f seconds\n", getdtime() - time);

   /* now load the compiled library */
   void *handle = lt_dlopen(filename);
   unlink(filename);
   if(!handle)
      die("dlopen failed: %s\n", lt_dlerror());
   return handle;
}

void *makeequationfunc(const char *equation)
{
   char filename[] = "/tmp/meteorcXXXXXX";
   int fd;
   if((fd = mkstemp(filename) < 0))
      die("mkstemp failed\n");
   FILE *file;
   if(!(file = fopen(filename, "w")))
      die("failed to open %s: %s\n", filename, strerror(errno));

   char *equal = strchr(equation, '=');
   char name[128];
   if(equal) {
      snprintf(name, sizeof name, "'%s'", equation);
      *equal = '-';
   } else
      snprintf(name, sizeof name, "'%s=0'", equation);

   fprintf(file, "#include <math.h>\ndouble func(double x, double y, "
           "double z){return (%s);}\n", equation);
   fclose(file);

   void *handle = compileandload(filename, name);
   unlink(filename);
   return lt_dlsym(handle, "func");
}
#endif

static void usage(void)
{
   printf(
  "Usage: meteor [OPTION]... [FILE]\n"
  "meteor generates a triangle mesh from an equation\n"
  "\nInformation\n"
  "-h, --help display this message\n"
  "    --keys information about keys during interactive display\n"
  "-q, --quiet hide console output\n"
  "    --version  print version and exit\n"
  "\nFile Options:\n"
  "-c, --create [FILE] save output to FILE\n"
  "-f, --file [FILE] read from file instead of generating\n"
  "    --input-format [FORMAT] specify a format of 'help' to list formats\n"
  "    --output-format [FORMAT] specify a format of 'help' to list formats\n"
  "\nMesh Generation Options:\n"
  "-a, --animate  rebuild the meteor each frame, optionally calling 'update'\n"
  "-e, --equation specify an equation to use instead of file\n"
  "-s, --step step size\n"
  "-[xyz] min,max  set the range for x, y, or z axis, default -1,1\n"
  "    --max-frames [NUM] abort after num frames have been generated\n"
  "    --max-triangles [NUM] max number of triangles to allow while building\n"
  "                    (saves ram).\n"
  "\nSimplification Options:\n"
  "-t, --triangles [NUM] or [NUM%] merge edges attempting to have NUM"
  " triangles\n\tremaining\n"
  "-j, --aggregation [NUM] perform aggregation on the mesh until there are not "
  "more\n       than NUM points remaining\n"
  "    --clip [EQUATION] clip the mesh by this equation\n"
  "    --correct-texcoords generate multiple points in the same location with"
  "\n\tcorrecting texture mapping errors\n"
  "-r, --propagate [ITERS] move points toward input function after generation\n"
  "\nTransformation Options:\n"
  "    --rotate angle,x,y,z  Rotate all points and normals by angle in "
  "degrees \n         around the vector <x,y,z>\n"
  "    --translate x,y,z  Translate all points by the vector <x,y,z>\n"
  "    --scale x,y,z  scale all points with the origin by <x,y,z>\n"
  "\nDisplay Options:\n"
  "-k, --keypress [KEY] pass a keyboard input to the program at startup\n"
  "-o, --no-normals\n"
  "-n, --no-display do not display\n"
  "    --texture [FILE] use image file for texture\n"
  "    --3D-texture the texture is a 3d texture\n"
  "    --osmesa [FILE] use libOSMesa to output to a file (png, or mp4)\n"
  "-g, --geometry WxH specify size, (also for off screen)\n"
  "    --loop when running animations with -f for input\n"
  );

   exit(0);
}

static void keys(void)
{
   printf("Keys at runtime:\n"
          "arrow keys, rotate x,y axis\n"
          "insert/delete, rotate z axis\n"
          "w,a,s,d translate on x,y axis\n"
          "pageup/pagedown, translate on z axis\n"
          "1,2 adjust far clip\n"
          "3,4 adjust near clip\n"
          "5,6 adjust field of view\n"
          "o toggle one/two sided lighting\n"
          "v begin video capture\n"
          "b end video capture\n"
          "n take screenshot\n"
          "l toggle wireframe mode\n"
          "c toggle backface culling\n"
          "f toggle flat shading\n"
          "m perform one edge reduction\n"
          "ESC or q, exit\n");

   exit(0);
}

static void version(void)
{
   puts(PACKAGE_STRING);
   exit(0);
}

static void setminmaxarg(double *min, double *max)
{
   if(sscanf(optarg, "%lf,%lf", min, max) != 2)
      die("invalid range: %s\n", optarg);
   if(*min >= *max)
      die("invalid range: %g,%g\n", *min, *max);
}

static void getrotation(void)
{
   if(sscanf(optarg, "%lf,%lf,%lf,%lf",
             Rotation+0, Rotation+1, Rotation+2, Rotation+3) != 4)
      die("invalid rotation: %s\n", optarg);
}

static void gettranslation(void)
{
   if(sscanf(optarg, "%lf,%lf,%lf",
             Translation+0, Translation+1, Translation+2) != 3)
      die("invalid translation: %s\n", optarg);
}

static void getscale(void)
{
   if(sscanf(optarg, "%lf,%lf,%lf",
             Scale+0, Scale+1, Scale+2) != 3)
      die("invalid scale: %s\n", optarg);
}

static double optdouble(const char *arg)
{
   char *endptr;
   double val = strtod(optarg, &endptr);
   if(*endptr != '\0')
      die("invalid value to %s: %s\n", arg, optarg);
   return val;
}

static int testformat(int input, int format)
{
   if(input)
      return meteorLoad(NULL, format) == 0;
   return meteorSave(NULL, format) == 0;
}

static int optformat(int input)
{
   int i;
   if(!strcmp(optarg, "help")) {
      printf("Supported formats: ");
      for(i = 0; i < formattablelen; i++)
         if(testformat(input, formattable[i].format))
            printf("%s ", formattable[i].name);
      if(input)
         printf("(default: auto-detect)\n");
      else
         printf("(default: text)\n");
      exit(0);
   }

   for(i = 0; i < formattablelen; i++)
      if(!strcmp(formattable[i].name, optarg))
         return formattable[i].format;

   const char *put = input ? "input" : "output";
   die("invalid %s format: %s\ntry --%s-format help\n", put, optarg, put);
}

static void opttriangles(void)
{
   char *endptr;
   double val = strtod(optarg, &endptr);
   if(*endptr == '\0')
       num_triangles = val;
    else
       if(*endptr == '%')
          percent_triangles = val;
       else
          die("invalid argument to --triangles (-t)\n");
}

static struct option longopts[] = {
   {"help", 0, 0, 'h'},
   {"keys", 0, 0, 1},
   {"quiet", 0, 0, 'q'},
   {"version", 0, 0, 2},
   /* file options */
   {"create", 1, 0, 'c'},
   {"file", 1, 0, 'f'},
   {"input-format", 1, 0, 3},
   {"output-format", 1, 0, 14},
   /* generation options */
   {"animate", 0, 0, 'a'},
   {"equation", 1, 0, 'e'},
   {"step", 1, 0, 's'},
   {"max-frames", 1, 0, 4},
   {"max-triangles", 1, 0, 15},
   /* simplification options */
   {"triangles", 1, 0, 't'},
   {"propagate", 1, 0, 'r'},
   {"aggregation", 1, 0, 'j'},
   {"clip", 1, 0, 5},
   {"correct-texcoords", 0, 0, 6},
    /* transformation options */
   {"rotate", 1, 0, 7},
   {"translate", 1, 0, 8},
   {"scale", 1, 0, 9},
    /* display options */
   {"keypress", 1, 0, 'k'},
   {"no-normals", 0, 0, 'o'},
   {"no-display", 0, 0, 'n'},
   {"texture", 1, 0, 10},
   {"3D-texture", 1, 0, 11},
   {"osmesa", 1, 0, 12},
   {"geometry", 1, 0, 'g'},
   {"loop", 0, 0, 13},
   {0, 0, 0, 0}};

int main(int argc, char** argv)
{
   char equation[PATH_MAX] = "";
   char createfilename[PATH_MAX] = "";
   char inputfilename[PATH_MAX] = "";
   char osmesafilename[PATH_MAX] = "";
   char clipequation[PATH_MAX] = "";

   double minx = -1, maxx = 1;
   double miny = -1, maxy = 1;
   double minz = -1, maxz = 1;
   double step = .05;
   int displaymeteor = 1;

   int c;

   void (*init)(void) = NULL;
   void (*normal)(double[3], double[3]) = NULL;
   void (*texcoord)(double[3], double[3]) = NULL;
   void (*color)(double[3], double[3]) = NULL;

   /* by default show console output */
   verbose = 1;

   for(;;) {
      switch(c = getopt_long(argc, argv, "hqae:s:x:y:z:m:r:k:onc:f:t:j:",
                             longopts, NULL)) {
      case 'h': usage();
      case 1: keys();
      case 'q': verbose = 0; break;
      case 2: version(); break;
         /* file options */
      case 'c': strncpy(createfilename, optarg, PATH_MAX); break;
      case 'f': strncpy(inputfilename, optarg, PATH_MAX); break;
      case 3: input_fileformat = optformat(1); break;
      case 14: output_fileformat = optformat(0); break;
         /* generation options */
      case 'a': animated = 1; break;
      case 'e': strncpy(equation, optarg, PATH_MAX); break;
      case 's': step = optdouble("step"); break;
      case 'x': setminmaxarg(&minx, &maxx); break;
      case 'y': setminmaxarg(&miny, &maxy); break;
      case 'z': setminmaxarg(&minz, &maxz); break;
      case 4: animationmaxframes = optdouble("max-frames"); break;
      case 15: max_num_triangles = optdouble("max-triangles"); break;
         /* simplification options */
      case 't': opttriangles(); break;
      case 'r': propagation = optdouble("propagation"); break;
      case 'j': meteoraggregation = optdouble("aggregation"); break;
      case 5: strncpy(clipequation, optarg, PATH_MAX); break;
      case 6: correcttexcoords = 1; break;
         /* transformation options */
      case 7: getrotation(); break;
      case 8: gettranslation(); break;
      case 9: getscale(); break;
         /* display options */
      case 'o': normals = 0; break;
      case 'n': displaymeteor = 0; break;
      case 12: strncpy(osmesafilename, optarg, PATH_MAX); break;
      case 13: animationloopmode = 1; break;
      case -1:
         goto nomoreargs;
      default:
         openglParseArgs(c);
      }
   }

   nomoreargs:

   /* send verbose messages to stderr */
   if(osmesafilename[0] && !strcmp(osmesafilename, "-") && verbose) 
      verbose = 2;

   if(createfilename[0]) {
      if(strcmp(createfilename, "-"))
         outputfile = fopen(createfilename, "w");
      else {
         if(verbose == 2)
            warning("Cannot output to stdout, stream is already used\n");
         else {
            outputfile = stdout;
            if(verbose) /* send verbose messages to stderr */
               verbose = 2;
         }
      }
   }

   int inputfilehastexture = 0;
   if(inputfilename[0]) {
      if(strcmp(inputfilename, "-")) {
         if((inputfile = fopen(inputfilename, "r")) == NULL)
            die("Failed to open '%s': %s\n", inputfilename, strerror(errno));
      } else
         inputfile = stdin;

      load();
      int format = meteorFormat();
      if(!(format & METEOR_NORMALS) && normals)
         warning("no normal data\n");

      inputfilehastexture = format & METEOR_TEXCOORDS;
   }

#ifndef HAVE_LIBLTDL
   if(inputfilename[0])
      goto transform;
   die("Compiled without libltdl, so it is not possible "
       "to generate a mesh from an input file, try --help\n");
#else

   lt_dlinit();

   /* compile and load the clipping equation if specified */
   if(clipequation[0])
      *(void **)(&clipfunc) = makeequationfunc(clipequation);

   if(inputfilename[0]) {
      if(equation[0])
         warning("Input data file specified with equation, using input data file\n");
      if(argv[optind])
         warning("Input data file specified with input source file, using input data file\n");
      goto transform;
   }

   if(equation[0])
      *(void **)(&func) = makeequationfunc(equation);

   /* now load the input source */
   char *sourcefilename = argv[optind];
   if(!sourcefilename)
      if(!equation[0])
         die("input source file missing, and "
             "no equation specified, try --help\n");
      else
         goto noinputsource;

   void *handle = compileandload(sourcefilename, sourcefilename);

   /* look for the init function */
   if(*(void **)(&init) = lt_dlsym(handle, "init"))
      init(); /* call it */

   /* load the meteor generation function */
   void *func2 = lt_dlsym(handle, "func");
   if(func) {
      if(func2)
         warning("--equation specified and 'func' exits in source file, "
                 "using source file\n");
   } else
      if(func2)
         *(void **)(&func) = func2;
      else
         die("Could not find 'func' in input file\n");

   /* look for the normal function */
   *(void **)(&normal) = lt_dlsym(handle, "normal");

   /* look for the color function */
   *(void **)(&color) = lt_dlsym(handle, "color");

   /* look for the texcoord function */
   *(void **)(&texcoord) = lt_dlsym(handle, "texcoord");

   /* look for the update function */
   *(void **)(&updatefunc) = lt_dlsym(handle, "update");

   /* look for the clipping function */
   void *clip = lt_dlsym(handle, "clip");
   if(clipfunc && clip)
      warning("overriding command line clip function with input file clip function\n");
   *(void **)(&clipfunc) = clip;

 noinputsource:

   meteorFunc(func);

   if(normal)
      meteorNormalFunc(normal);
   else {
      verbose_printf("No normal function, will use input function to"
                     " approximate\n");
      meteorNormalFunc(defaultnormal);
   }

   meteorTexCoordFunc(texcoord);
   meteorColorFunc(color);

   /* set up the meteor for generation */
   meteorSetSize(minx, maxx, miny, maxy, minz, maxz, step);

   int format = (!!normals)*METEOR_NORMALS | (!!color)*METEOR_COLORS
      | (!!texcoord)*METEOR_TEXCOORDS;
   meteorReset(format);
   build();

   /* warn about potentially invalid combinations */
   if(animated && !updatefunc)
      warning("animation on without an update function\n");

#endif /* HAVE_LIBLTDL */
 transform:

   if(inputfile && max_num_triangles != -1)
      warning("--max-triangles only useful when generating a meteor\n");

   transformmeteor();

   if(outputfile) {
      save();
      if(!animated && outputfile != stdout)
         fclose(outputfile);
   }

 display:
   info();

   /* warn about potentially invalid combinations */
   if(!(animated && inputfile) && animationloopmode)
      warning("--loop has no effect without --file and --animate\n");
   if(!animated && animationmaxframes != -1)
      warning("--max-frames has no effect without --animate\n");

   if(!displaymeteor) {
      if(osmesafilename[0])
         warning("--osmesa has no effect with --no-display\n");
      if(animated)
         nodisplayMainLoop();
      return 0;
   }

   if((texcoord || inputfilehastexture) && !texturefilename[0])
      die("Error: texture function exists, and no texture image provided\n");
   if(texturefilename[0] && !(texcoord || inputfilehastexture)) {
      if(!texcoord)
         warning("texture image, but no texture function\n");
      else
      if(!inputfilehastexture)
         warning("texture image, but no texture data\n");
   }

   /* run graphics */
   if(osmesafilename[0]) {
      osmesaMainLoop(osmesafilename);
   } else {
      /* convert arguments for glut */
      argc -= optind - 1;
      char *newargv[argc + 2];
      newargv[0] = argv[0];
      memcpy(newargv + 1, argv + optind, (argc + 1) * sizeof(*argv));

      startglut(&argc, newargv);
   }
   return 0;
}
