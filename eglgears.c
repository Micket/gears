/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This is a port of the infamous "glxgears" demo to straight EGL
 *
 * See usage() below for command line options.
 */

//#define EGL_EGLEXT_PROTOTYPES

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define MAX_CONFIGS 10

#define BENCHMARK

#ifdef BENCHMARK

/* XXX this probably isn't very portable */

#include <sys/time.h>
#include <unistd.h>

/* return current time (in seconds) */
static double
current_time(void)
{
   struct timeval tv;
#ifdef __VMS
   (void) gettimeofday(&tv, NULL );
#else
   struct timezone tz;
   (void) gettimeofday(&tv, &tz);
#endif
   return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
}

#else /*BENCHMARK*/

/* dummy */
static double
current_time(void)
{
   /* update this function for other platforms! */
   static double t = 0.0;
   static int warn = 1;
   if (warn) {
      fprintf(stderr, "Warning: current_time() not implemented!!\n");
      warn = 0;
   }
   return t += 1.0;
}

#endif /*BENCHMARK*/



#ifndef M_PI
#define M_PI 3.14159265
#endif


/** Event handler results: */
#define NOP 0
#define EXIT 1
#define DRAW 2

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static GLboolean fullscreen = GL_FALSE; /* Create a single fullscreen window */
static GLboolean stereo = GL_FALSE;     /* Enable stereo.  */
static GLint samples = 0;               /* Choose visual with at least N samples. */
static GLboolean animate = GL_TRUE;     /* Animation */
static GLfloat eyesep = 5.0;            /* Eye separation. */
static GLfloat fix_point = 40.0;        /* Fixation point distance.  */
static GLfloat left, right, asp;        /* Stereo frustum params.  */


/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 * 
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
   GLint i;
   GLfloat r0, r1, r2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth / 2.0;
   r2 = outer_radius + tooth_depth / 2.0;

   da = 2.0 * M_PI / teeth / 4.0;

   glShadeModel(GL_FLAT);

   glNormal3f(0.0, 0.0, 1.0);

   /* draw front face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      if (i < teeth) {
         glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    width * 0.5);
      }
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
   }
   glEnd();

   glNormal3f(0.0, 0.0, -1.0);

   /* draw back face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      if (i < teeth) {
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    -width * 0.5);
         glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      }
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
   }
   glEnd();

   /* draw outward faces of teeth */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
   }

   glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
   glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

   glEnd();

   glShadeModel(GL_SMOOTH);

   /* draw inside radius cylinder */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
   }
   glEnd();
}


static void
draw(void)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glRotatef(view_rotx, 1.0, 0.0, 0.0);
   glRotatef(view_roty, 0.0, 1.0, 0.0);
   glRotatef(view_rotz, 0.0, 0.0, 1.0);

   glPushMatrix();
   glTranslatef(-3.0, -2.0, 0.0);
   glRotatef(angle, 0.0, 0.0, 1.0);
   glCallList(gear1);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(3.1, -2.0, 0.0);
   glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
   glCallList(gear2);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(-3.1, 4.2, 0.0);
   glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
   glCallList(gear3);
   glPopMatrix();

   glPopMatrix();
}


static void
draw_gears(void)
{
   if (stereo) {
      /* First left eye.  */
      glDrawBuffer(GL_BACK_LEFT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(left, right, -asp, asp, 5.0, 60.0);

      glMatrixMode(GL_MODELVIEW);

      glPushMatrix();
      glTranslated(+0.5 * eyesep, 0.0, 0.0);
      draw();
      glPopMatrix();

      /* Then right eye.  */
      glDrawBuffer(GL_BACK_RIGHT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-right, -left, -asp, asp, 5.0, 60.0);

      glMatrixMode(GL_MODELVIEW);

      glPushMatrix();
      glTranslated(-0.5 * eyesep, 0.0, 0.0);
      draw();
      glPopMatrix();
   }
   else {
      draw();
   }
}


/** Draw single frame, do SwapBuffers, compute FPS */
static void
draw_frame(EGLDisplay egl_dpy, EGLSurface surf)
{
   static int frames = 0;
   static double tRot0 = -1.0, tRate0 = -1.0;
   double dt, t = current_time();

   if (tRot0 < 0.0)
      tRot0 = t;
   dt = t - tRot0;
   tRot0 = t;

   if (animate) {
      /* advance rotation for next frame */
      angle += 70.0 * dt;  /* 70 degrees per second */
      if (angle > 3600.0)
         angle -= 3600.0;
   }

   //draw_gears();
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glBegin(GL_TRIANGLES);
   glVertex3f(-1.0f, -1.0f, 0.0f);
   glVertex3f( 1.0f, -1.0f, 0.0f);
   glVertex3f( 0.0f,  1.2f, 0.0f);
   glEnd();
   
   eglSwapBuffers(egl_dpy, surf);

   frames++;
   
   if (tRate0 < 0.0)
      tRate0 = t;
   if (t - tRate0 >= 5.0) {
      GLfloat seconds = t - tRate0;
      GLfloat fps = frames / seconds;
      printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
             fps);
      fflush(stdout);
      tRate0 = t;
      frames = 0;
   }
}


/* new window size or exposure */
static void
reshape(int width, int height)
{
   glViewport(0, 0, (GLint) width, (GLint) height);

   if (stereo) {
      GLfloat w;

      asp = (GLfloat) height / (GLfloat) width;
      w = fix_point * (1.0 / 5.0);

      left = -5.0 * ((w - 0.5 * eyesep) / fix_point);
      right = 5.0 * ((w + 0.5 * eyesep) / fix_point);
   }
   else {
      GLfloat h = (GLfloat) height / (GLfloat) width;

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
   }
   
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.0, 0.0, -40.0);
}
   


static void
init(void)
{
   static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   glEnable(GL_CULL_FACE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_DEPTH_TEST);

   /* make the gears */
   gear1 = glGenLists(1);
   glNewList(gear1, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
   gear(1.0, 4.0, 1.0, 20, 0.7);
   glEndList();

   gear2 = glGenLists(1);
   glNewList(gear2, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
   gear(0.5, 2.0, 2.0, 10, 0.7);
   glEndList();

   gear3 = glGenLists(1);
   glNewList(gear3, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
   gear(1.3, 2.0, 0.5, 10, 0.7);
   glEndList();

   glEnable(GL_NORMALIZE);
}


/**
 * Remove window border/decorations.
 */
static void
no_border( Display *dpy, Window w)
{
   static const unsigned MWM_HINTS_DECORATIONS = (1 << 1);
   static const int PROP_MOTIF_WM_HINTS_ELEMENTS = 5;

   typedef struct
   {
      unsigned long       flags;
      unsigned long       functions;
      unsigned long       decorations;
      long                inputMode;
      unsigned long       status;
   } PropMotifWmHints;

   PropMotifWmHints motif_hints;
   Atom prop, proptype;
   unsigned long flags = 0;

   /* setup the property */
   motif_hints.flags = MWM_HINTS_DECORATIONS;
   motif_hints.decorations = flags;

   /* get the atom for the property */
   prop = XInternAtom( dpy, "_MOTIF_WM_HINTS", True );
   if (!prop) {
      /* something went wrong! */
      return;
   }

   /* not sure this is correct, seems to work, XA_WM_HINTS didn't work */
   proptype = prop;

   XChangeProperty( dpy, w,                         /* display, window */
                    prop, proptype,                 /* property, type */
                    32,                             /* format: 32-bit datums */
                    PropModeReplace,                /* mode */
                    (unsigned char *) &motif_hints, /* data */
                    PROP_MOTIF_WM_HINTS_ELEMENTS    /* nelements */
                  );
}


/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void
make_window( Display *dpy, EGLDisplay egl_dpy, const char *name,
             int x, int y, int width, int height,
             Window *winRet, EGLContext *ctxRet, EGLSurface *surfRet)
{
   int i = 0;

   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   EGLContext ctx;
   EGLint maj, min;
   EGLSurface surf;
   EGLConfig configs[MAX_CONFIGS];
   EGLConfig config;
   EGLint numConfigs;
   EGLint vid;
   int num_visuals;
   XVisualInfo *visinfo;
   XVisualInfo vistemplate;
   
   EGLint attribs[] = {
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
      EGL_RED_SIZE,        1,
      EGL_GREEN_SIZE,      1,
      EGL_BLUE_SIZE,       1,
      // EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, /* If one needs GLES2 */
      EGL_NONE
   };
   
   EGLint ctxattribs[] = {
       EGL_CONTEXT_CLIENT_VERSION, 2,
       EGL_NONE
    };

#if 0
   /* Singleton attributes. */
   attribs[i++] = GLX_RGBA;
   attribs[i++] = GLX_DOUBLEBUFFER;
   if (stereo)
      attribs[i++] = GLX_STEREO;

   /* Key/value attributes. */
   attribs[i++] = GLX_RED_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_GREEN_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_BLUE_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_DEPTH_SIZE;
   attribs[i++] = 1;
   if (samples > 0) {
      attribs[i++] = GLX_SAMPLE_BUFFERS;
      attribs[i++] = 1;
      attribs[i++] = GLX_SAMPLES;
      attribs[i++] = samples;
   }

   attribs[i++] = None;
#endif

   scrnum = DefaultScreen( dpy );
   root = RootWindow( dpy, scrnum );

   if (!eglInitialize(egl_dpy, &maj, &min)) {
      printf("eglgears: eglInitialize failed: %x\n", eglGetError());
      exit(1);
   }
   printf("eglgears: EGL version %d.%d initialized\n", maj, min);

#if 1
   eglGetConfigs(egl_dpy, configs, MAX_CONFIGS, &numConfigs);
   for (i = 0; i < numConfigs; ++i) {
      EGLint surf_type, samples;
      eglGetConfigAttrib(egl_dpy, configs[i], EGL_SURFACE_TYPE, &surf_type);
      eglGetConfigAttrib(egl_dpy, configs[i], EGL_SAMPLES, &samples);
      
      printf("eglgears: configs[%d]: surf_type=%d, samples=%d\n", i, surf_type, samples);
   }
   config = configs[0];
#else
   /* create EGL rendering context */
   if ( !eglChooseConfig( egl_dpy, attribs, &config, 1, &numConfigs ) ) {
      printf("Failed to choose config (eglError: %x)\n", eglGetError());
      exit(1);
   }

   if ( numConfigs != 1 ) {
      exit(1);
   }
#endif

   if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
      printf("Error: eglGetConfigAttrib() failed\n");
      exit(1);
   }

   /* The X window visual must match the EGL config */
   vistemplate.visualid = vid;
   visinfo = XGetVisualInfo(dpy, VisualIDMask, &vistemplate, &num_visuals);
   if (!visinfo) {
      printf("Error: couldn't get X visual\n");
      exit(1);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   /* XXX this is a bad way to get a borderless window! */
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, x, y, width, height,
                        0, visinfo->depth, InputOutput,
                        visinfo->visual, mask, &attr );

   if (fullscreen)
      no_border(dpy, win);

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(dpy, win, &sizehints);
      XSetStandardProperties(dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   ctx = eglCreateContext( egl_dpy, config, EGL_NO_CONTEXT, ctxattribs);
   if (!ctx) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }

   surf = eglCreateWindowSurface ( egl_dpy, config, win, NULL );
   if (surf == EGL_NO_SURFACE ) {
      printf("Error: CreateWindowSurface, EGL error: %d\n", eglGetError() );
      exit(1);
   }

   XFree(visinfo);

   *winRet = win;
   *ctxRet = ctx;
   *surfRet = surf;
}

#if 0
/**
 * Determine whether or not a GLX extension is supported.
 */
static int
is_glx_extension_supported(Display *dpy, const char *query)
{
   const int scrnum = DefaultScreen(dpy);
   const char *glx_extensions = NULL;
   const size_t len = strlen(query);
   const char *ptr;

   if (glx_extensions == NULL) {
      glx_extensions = glXQueryExtensionsString(dpy, scrnum);
   }

   ptr = strstr(glx_extensions, query);
   return ((ptr != NULL) && ((ptr[len] == ' ') || (ptr[len] == '\0')));
}


/**
 * Attempt to determine whether or not the display is synched to vblank.
 */
static void
query_vsync(Display *dpy, GLXDrawable drawable)
{
   int interval = 0;

#if defined(GLX_EXT_swap_control)
   if (is_glx_extension_supported(dpy, "GLX_EXT_swap_control")) {
       unsigned int tmp = -1;
       glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &tmp);
       interval = tmp;
   } else
#endif
   if (is_glx_extension_supported(dpy, "GLX_MESA_swap_control")) {
      PFNGLXGETSWAPINTERVALMESAPROC pglXGetSwapIntervalMESA =
          (PFNGLXGETSWAPINTERVALMESAPROC)
          glXGetProcAddressARB((const GLubyte *) "glXGetSwapIntervalMESA");

      interval = (*pglXGetSwapIntervalMESA)();
   } else if (is_glx_extension_supported(dpy, "GLX_SGI_swap_control")) {
      /* The default swap interval with this extension is 1.  Assume that it
       * is set to the default.
       *
       * Many Mesa-based drivers default to 0, but all of these drivers also
       * export GLX_MESA_swap_control.  In that case, this branch will never
       * be taken, and the correct result should be reported.
       */
      interval = 1;
   }


   if (interval > 0) {
      printf("Running synchronized to the vertical refresh.  The framerate should be\n");
      if (interval == 1) {
         printf("approximately the same as the monitor refresh rate.\n");
      } else if (interval > 1) {
         printf("approximately 1/%d the monitor refresh rate.\n",
                interval);
      }
   }
}
#endif

/**
 * Handle one X event.
 * \return NOP, EXIT or DRAW
 */
static int
handle_event(Display *dpy, Window win, XEvent *event)
{
   (void) dpy;
   (void) win;

   switch (event->type) {
   case Expose:
      return DRAW;
   case ConfigureNotify:
      reshape(event->xconfigure.width, event->xconfigure.height);
      break;
   case KeyPress:
      {
         char buffer[10];
         int code;
         code = XLookupKeysym(&event->xkey, 0);
         if (code == XK_Left) {
            view_roty += 5.0;
         }
         else if (code == XK_Right) {
            view_roty -= 5.0;
         }
         else if (code == XK_Up) {
            view_rotx += 5.0;
         }
         else if (code == XK_Down) {
            view_rotx -= 5.0;
         }
         else {
            XLookupString(&event->xkey, buffer, sizeof(buffer),
                          NULL, NULL);
            if (buffer[0] == 27) {
               /* escape */
               return EXIT;
            }
            else if (buffer[0] == 'a' || buffer[0] == 'A') {
               animate = !animate;
            }
         }
         return DRAW;
      }
   }
   return NOP;
}


static void
event_loop(Display *dpy, Window win, EGLDisplay egl_dpy, EGLSurface surf, EGLContext ctx)
{
   float x = 0.0;;
   while (1) {
      int op;
      /*
      while (!animate || XPending(dpy) > 0) {
         XEvent event;
         XNextEvent(dpy, &event);
         op = handle_event(dpy, win, &event);
         if (op == EXIT)
            return;
         else if (op == DRAW)
            break;
      }*/

      //x += 0.01;
      if (x > 1.0) { x = 0.; }
      glClearColor(0.5, 0.5, x, 0.0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glBegin(GL_TRIANGLES);
      glVertex3f(-1.0f, -1.0f, 0.0f);
      glVertex3f( 1.0f, -1.0f, 0.0f);
      glVertex3f( 0.0f,  1.2f, 0.0f);
      glEnd();
      eglSwapBuffers(egl_dpy, surf);

      //draw_frame(egl_dpy, surf);
   }
}


static void
usage(void)
{
   printf("Usage:\n");
   printf("  -display <displayname>  set the display to run on\n");
   printf("  -stereo                 run in stereo mode\n");
   printf("  -samples N              run in multisample mode with at least N samples\n");
   printf("  -fullscreen             run in fullscreen mode\n");
   printf("  -info                   display OpenGL renderer info\n");
   printf("  -geometry WxH+X+Y       window geometry\n");
}
 

int
main(int argc, char *argv[])
{
   unsigned int winWidth = 300, winHeight = 300;
   int x = 0, y = 0;
   Display *dpy;
   Window win;
   EGLContext ctx;
   EGLDisplay egl_dpy;
   EGLSurface surf;
   char *dpyName = NULL;
   GLboolean printInfo = GL_FALSE;
   int i;

   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-display") == 0) {
         dpyName = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-info") == 0) {
         printInfo = GL_TRUE;
      }
      else if (strcmp(argv[i], "-stereo") == 0) {
         stereo = GL_TRUE;
      }
      else if (i < argc-1 && strcmp(argv[i], "-samples") == 0) {
         samples = strtod(argv[i+1], NULL );
         ++i;
      }
      else if (strcmp(argv[i], "-fullscreen") == 0) {
         fullscreen = GL_TRUE;
      }
      else if (i < argc-1 && strcmp(argv[i], "-geometry") == 0) {
         XParseGeometry(argv[i+1], &x, &y, &winWidth, &winHeight);
         i++;
      }
      else {
         usage();
         return -1;
      }
   }

   dpy = XOpenDisplay(dpyName);
   if (!dpy) {
      printf("Error: couldn't open display %s\n",
             dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
   }

   egl_dpy = eglGetDisplay( (EGLNativeDisplayType) dpy );
   if ( egl_dpy == EGL_NO_DISPLAY ) {
      printf("Error: could not get EGL display\n");
      return -1;
   }

   if (fullscreen) {
      int scrnum = DefaultScreen(dpy);

      x = 0; y = 0;
      winWidth = DisplayWidth(dpy, scrnum);
      winHeight = DisplayHeight(dpy, scrnum);
   }

   make_window(dpy, egl_dpy, "eglgears", x, y, winWidth, winHeight, &win, &ctx, &surf);
   XMapWindow(dpy, win);

   eglMakeCurrent(egl_dpy, surf, surf, ctx);
   //query_vsync(dpy, win); // TODO

   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
   }

   init();

   /* Set initial projection/viewing transformation.
    * We can't be sure we'll get a ConfigureNotify event when the window
    * first appears.
    */
   reshape(winWidth, winHeight);

   glDrawBuffer( GL_BACK );
   event_loop(dpy, win, egl_dpy, surf, ctx);

   glDeleteLists(gear1, 1);
   glDeleteLists(gear2, 1);
   glDeleteLists(gear3, 1);
   eglDestroySurface(egl_dpy, surf);
   eglDestroyContext(egl_dpy, ctx);
   eglTerminate(egl_dpy);
 
   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);

   return 0;
}

#if 0
int
main(int argc, char *argv[])
{
      int maj, min;
      EGLContext ctx;
      EGLSurface screen_surf;
      EGLConfig configs[MAX_CONFIGS];
      EGLint numConfigs, i;
      EGLBoolean b;
      EGLDisplay d;
      EGLint screenAttribs[10];
      EGLModeMESA mode[MAX_MODES];
      EGLScreenMESA screen;
      EGLint count, chosenMode;
      GLboolean printInfo = GL_FALSE;
      EGLint width = 0, height = 0;
      
        /* parse cmd line args */
      for (i = 1; i < argc; i++)
      {
            if (strcmp(argv[i], "-info") == 0)
            {
                  printInfo = GL_TRUE;
            }
            else
                  printf("Warning: unknown parameter: %s\n", argv[i]);
      }
      
      /* DBR : Create EGL context/surface etc */
      d = eglGetDisplay((EGLNativeDisplayType)"!egl_kms_vmwgfx");
      assert(d);

      if (!eglInitialize(d, &maj, &min)) {
            printf("eglgears: eglInitialize failed: %d\n", eglGetError());
            exit(1);
      }
      
      printf("eglgears: EGL version = %d.%d\n", maj, min);
      printf("eglgears: EGL_VENDOR = %s\n", eglQueryString(d, EGL_VENDOR));
      
        /* XXX use ChooseConfig */
      eglGetConfigs(d, configs, MAX_CONFIGS, &numConfigs);
      eglGetScreensMESA(d, &screen, 1, &count);

      if (!eglGetModesMESA(d, screen, mode, MAX_MODES, &count) || count == 0) {
            printf("eglgears: eglGetModesMESA failed! %d --\n", eglGetError());
            return 0;
      }

        /* Print list of modes, and find the one to use */
      printf("eglgears: Found %d modes:\n", count);
      for (i = 0; i < count; i++) {
            EGLint w, h;
            eglGetModeAttribMESA(d, mode[i], EGL_WIDTH, &w);
            eglGetModeAttribMESA(d, mode[i], EGL_HEIGHT, &h);
            printf("%3d: %d x %d\n", i, w, h);
            if (w > width && h > height) {
                  width = w;
                  height = h;
                        chosenMode = i;
            }
      }
      printf("eglgears: Using screen mode/size %d: %d x %d\n", chosenMode, width, height);

      ctx = eglCreateContext(d, configs[0], EGL_NO_CONTEXT, NULL);
      if (ctx == EGL_NO_CONTEXT) {
            printf("eglgears: failed to create context\n");
            return 0;
      }
      
      /* build up screenAttribs array */
      i = 0;
      screenAttribs[i++] = EGL_WIDTH;
      screenAttribs[i++] = width;
      screenAttribs[i++] = EGL_HEIGHT;
      screenAttribs[i++] = height;
      screenAttribs[i++] = EGL_NONE;

      screen_surf = eglCreateScreenSurfaceMESA(d, configs[0], screenAttribs);
      if (screen_surf == EGL_NO_SURFACE) {
            printf("eglgears: failed to create screen surface\n");
            return 0;
      }
      
      b = eglShowScreenSurfaceMESA(d, screen, screen_surf, mode[chosenMode]);
      if (!b) {
            printf("eglgears: show surface failed\n");
            return 0;
      }

      b = eglMakeCurrent(d, screen_surf, screen_surf, ctx);
      if (!b) {
            printf("eglgears: make current failed\n");
            return 0;
      }
      
      if (printInfo)
      {
            printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
            printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
            printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
            printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
      }
      
      init();
      reshape(width, height);

      glDrawBuffer( GL_BACK );

      run_gears(d, screen_surf, 5.0);
      
      eglDestroySurface(d, screen_surf);
      eglDestroyContext(d, ctx);
      eglTerminate(d);
      
      return 0;
}
#endif
