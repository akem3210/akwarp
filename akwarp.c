/* ---------------------------------------------------------------- *\

  file    : ewmh_ws_warp.c
  author  : m. gumz <akira at fluxbox dot org>
  copyr   : copyright (c) 2005 by m. gumz

  license : 

      Permission is hereby granted, free of charge, to any person
      obtaining a copy of this software and associated documentation
      files (the "Software"), to deal in the Software without
      restriction, including without limitation the rights to use,
      copy, modify, merge, publish, distribute, sublicense, and/or
      sell copies of the Software, and to permit persons to whom
      the Software is furnished to do so, subject to the following
      conditions:

      The above copyright notice and this permission notice shall be
      included in all copies or substantial portions of the Software.

      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
      OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
      NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
      HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
      WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
      FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
      OTHER DEALINGS IN THE SOFTWARE.
  
  
  start   : Mi 25 Mai 2005 09:59:12 CEST

  $Id: akwarp.c 11 2005-10-07 05:45:47Z mathias $
\* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- *\

  about : 
 
    if the mousepointer enters one of either nextworkspace-area or
    prevworkspacearea and stay there for longer than a given 
    activation_time, akwarp will change to the next/prev workspace,
    using ewmh_standards... which means you need an ewmh-compatible
    windowmanager (eg fluxbox)

    mouseposition is checked at ~ 25fps, that should be good enough.

    see README.txt for more information

\* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- *\
  includes
\* ---------------------------------------------------------------- */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef HAVE_XRENDER
#    include <X11/extensions/Xrender.h>
#endif /* HAVE_XRENDER */
#ifdef HAVE_XSHAPE
#    include <X11/extensions/shape.h>
#endif /* HAVE_XSHAPE */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <signal.h>

/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */

#define PROGRAM "akwarp"
#ifndef VERSION
#    define VERSION "1.0rc1"
#endif /* VERSION */

#define ERRMSG(msg) { printf("%s: error, %s\n",  PROGRAM, msg); }

static Display* dpy = NULL;
static Window root = None;
static int scrn = 0;
static unsigned int width = 0;
static unsigned int height = 0;

static Bool show_buttons = False;
static XColor button_color;
#ifdef HAVE_XRENDER
static Bool do_shading = False;
static int shade = 50;
#endif /* HAVE_XRENDER */

static float activation_time = 1.0f;

static Atom atom_net_current_desktop = None;
static Atom atom_net_number_of_desktops = None;

typedef struct {
    XRectangle area;
    Window area_win;
    Pixmap area_bg;
    Bool area_drawn;
#ifdef HAVE_XSHAPE
    Pixmap area_shape_pm;
#endif /* HAVE_XSHAPE */
    struct timeval first_time_over_area;
    Bool first_touch_area;
    int change_ws_by;
} warp_button;

static warp_button prev_ws, next_ws, *buttons[2];

/*------------------------------------------------------------------*\
\*------------------------------------------------------------------*/

#define FREE_PIXMAP(pm) { if (pm) XFreePixmap(dpy, pm); }
#define FREE_WINDOW(win) { if (win) XDestroyWindow(dpy, win); }

static void at_exit() {
#ifdef HAVE_XSHAPE
    FREE_PIXMAP(prev_ws.area_shape_pm);
    FREE_PIXMAP(next_ws.area_shape_pm);
#endif /* HAVE_XSHAPE */
    FREE_PIXMAP(prev_ws.area_bg);
    FREE_PIXMAP(next_ws.area_bg);
    FREE_WINDOW(prev_ws.area_win);
    FREE_WINDOW(next_ws.area_win);
    
    if (dpy)
        XCloseDisplay(dpy);
}

static void at_signal(int signal) {
    exit(0);
}

static void display_usage() {
    printf("\nUsage:\n  %s [-h] [-pws <geometry>] [-nws <geometry>] [-t <timeout>]\n", 
            PROGRAM);

    printf("\nOptions:\n"
           "  -h              - displays help\n"
           "  -v              - displays version\n"
           "  -pws <geometry> - define area for 'previous workspace',\n"
           "                    default is 10xheight+0+0\n"
           "  -nws <geometry> - define area for 'next workspace',\n"
           "                    default is 10xheight-10+0\n"
           "  -t <time>       - define timeout as float\n"
           "  -b              - show buttons\n"
           "  -c <color>      - color of the buttons\n"
#ifdef HAVE_XRENDER
           "  -s <int>        - shading of the buttons\n"
#endif /* HAVE_XRENDER */
#ifdef HAVE_XSHAPE
           "  -spws <file>    - shapefile for 'prev' button\n"
           "  -snws <file>    - shapefile for 'next' button\n"
#endif /* HAVE_XSHAPE */
           "\n"
           "  see XParseGeometry(3) for details about <geometry>.\n");
    printf("\nAuthor:\n"
           " Mathias Gumz <akira at fluxbox dot org> (C) 2005.\n\n");
}

/****
static const float tfactor = 1.0f / 1000000.0f;
static float time_diff(const struct timeval* a, const struct timeval* b) {
    return ((float)a->tv_sec + (float)(a->tv_usec) * tfactor) - 
           ((float)b->tv_sec + (float)(b->tv_usec) * tfactor);
}
****/

static void update_region(const char* regionstring, XRectangle* rect) {

    int x, y = 0;
    unsigned int w, h = 0;
    int mask = XParseGeometry(regionstring, &x, &y, &w, &h);

    if (mask & WidthValue && w > 0)
        rect->width = w;
    if (mask & HeightValue && h > 0)
        rect->height = h;

    if (mask & XValue)
        rect->x = (mask & XNegative) ? width + x : x;
    if (mask & YValue)
        rect->y = (mask & YNegative) ? height + y : y;

}

static int check_ewmh() {
    Atom atom_net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom atom_rtype;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long bytes;
    unsigned char* data = NULL;
    
    if (XGetWindowProperty(dpy, root, atom_net_supporting_wm_check,
                         0, 1, 
                         False, 0,
                         &atom_rtype, &format, &nitems, &bytes,
                         (unsigned char**)&data) == Success) {
        if (data)
            XFree(data);
        data = NULL;
        Atom atom_net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
        if (XGetWindowProperty(dpy, root, atom_net_supported,
                         0, 0x7fffffff, 
                         False, XA_ATOM,
                         &atom_rtype, &format, &nitems, &bytes,
                         (unsigned char**)&data) == Success) {
            if (data) {
                Atom* atoms = (Atom*)data;
                char** names = (char**)malloc(nitems * sizeof(char*));
                size_t i;
                Bool support_net_wm_current_desktop = False;
                Bool support_net_wm_number_of_desktops = False;

                XGetAtomNames(dpy, atoms, nitems, names);
                for (i = 0; i < nitems; i++) {
                    if (!strcmp(names[i], "_NET_NUMBER_OF_DESKTOPS"))
                        support_net_wm_number_of_desktops = True;
                    else if (!strcmp(names[i], "_NET_CURRENT_DESKTOP"))
                        support_net_wm_current_desktop = True;
                }
                free(names);
                XFree(data);
            
                if (support_net_wm_current_desktop && support_net_wm_number_of_desktops) {
                    atom_net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
                    atom_net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
                    return 1;
                }
                
                return 0;
            }
        }
    }

    return 0;
}

static void get_ws_info(int* current_ws, int* nr_ws) {
    int format = 0;
    unsigned long nitems = 0;
    unsigned long bytes;
    unsigned char* data = NULL;
    Atom atom_rtype;
    
    if (XGetWindowProperty(dpy, root, atom_net_current_desktop,
                         0, 1, 
                         False, 0,
                         &atom_rtype, &format, &nitems, &bytes,
                         (unsigned char**)&data) == Success) {

        if (data) {
            *current_ws = (int)(*data);
            XFree(data);
        }
    }

    data = NULL;
    
    if (XGetWindowProperty(dpy, root, atom_net_number_of_desktops,
                         0, 1, 
                         False, 0,
                         &atom_rtype, &format, &nitems, &bytes,
                         (unsigned char**)&data) == Success) {

        if (data) {
            *nr_ws = (int)(*data);
            XFree(data);
        }
    }
}

static void change_ws(unsigned int nr_ws) {

    XEvent event;
    unsigned long mask = SubstructureRedirectMask;

//printf("change workspace\n");


    event.xclient.type = ClientMessage;
    event.xclient.window = root;

    event.xclient.message_type = atom_net_current_desktop;
    event.xclient.format = 32;
    event.xclient.data.l[0] = (unsigned long)nr_ws;
    event.xclient.data.l[1] = CurrentTime;

    if (next_ws.area_win) {
        next_ws.area_drawn = False;
        XUnmapWindow(dpy, next_ws.area_win);
    }
    if (prev_ws.area_win) {
        prev_ws.area_drawn = False;
        XUnmapWindow(dpy, prev_ws.area_win);
    }
    
    XSendEvent(dpy, root, False, mask, &event);


/****
char wmctrl_call_string[14];
snprintf(&wmctrl_call_string[0], 14, "wmctrl -s %d\n", nr_ws);
system(wmctrl_call_string);
****/
//printf("ok");
}

static void change_ws_by_num(int step) {

    int current_ws = 0;
    int nr_ws = 1;
    int new_ws = 1;
    
    get_ws_info(&current_ws, &nr_ws);
    step %= nr_ws;
    new_ws = (current_ws + nr_ws + step) % nr_ws;

    if (new_ws != current_ws)
        change_ws(new_ws);

    prev_ws.first_touch_area = True;
    next_ws.first_touch_area = True;
}


static Window create_window(XRectangle* rect) {

    Window win = None;
    XSetWindowAttributes attr;

    attr.override_redirect = True;
    attr.colormap = DefaultColormap(dpy, scrn);
    attr.background_pixel = button_color.pixel;
    attr.border_pixel = BlackPixel(dpy, scrn);

    win = XCreateWindow(dpy, root,
                          rect->x, rect->y, rect->width, rect->height,
                          0, /* borderwidth */
                          CopyFromParent, /* depth */
                          InputOutput, /* class */
                          CopyFromParent, /* visual */
                          CWBackPixel|CWColormap|CWOverrideRedirect, &attr);
    return win;
}

static Bool check_xrender() {
#ifdef HAVE_XRENDER
    static Bool have_xrender = False;
    static Bool checked_already = False;

    if(!checked_already) {
        int major_opcode, first_event, first_error;
        have_xrender = (XQueryExtension(dpy, "RENDER",
                            &major_opcode,
                            &first_event, &first_error) == False);
        
        checked_already = True;
    }
    
    return have_xrender;
#else
    return False;
#endif /* HAVE_XRENDER */
}

static void shade_window(Window win, Pixmap* bg_pm, XRectangle* dim) {
#ifdef HAVE_XRENDER
    Picture shade_pic = None;
    XRenderPictFormat* format = None;
    XRenderPictFormat shade_format;
    Visual* vis = DefaultVisual(dpy, scrn);

    Pixmap src_pm;
    Pixmap dst_pm;

    { 
        XImage* image = XGetImage(dpy, root, dim->x, dim->y, dim->width, dim->height, AllPlanes, ZPixmap);
        src_pm = XCreatePixmap(dpy, root, dim->width, dim->height, DefaultDepth(dpy, scrn));
        XPutImage(dpy, src_pm, DefaultGC(dpy, scrn), image, 0, 0, 0, 0, dim->width, dim->height);
        XDestroyImage(image);
    }

    {
        if (*bg_pm)
            dst_pm = *bg_pm; 
        else
            dst_pm = XCreatePixmap(dpy, win, dim->width, dim->height, DefaultDepth(dpy, scrn));

        {
            GC gc;
            XGCValues val;
            val.foreground = button_color.pixel; 
            gc = XCreateGC(dpy, dst_pm, GCForeground, &val);
            XFillRectangle(dpy, dst_pm, gc, 0, 0, dim->width, dim->height);
            XFreeGC(dpy, gc);
        }
    }

    {
        XRenderPictFormat shade_format;
        unsigned long mask = PictFormatType|PictFormatDepth|PictFormatAlpha|PictFormatAlphaMask;
        shade_format.type = PictTypeDirect;
        shade_format.depth = 8;
        shade_format.direct.alpha = 0;
        shade_format.direct.alphaMask = 0xff;

        format = XRenderFindFormat(dpy, mask, &shade_format, 0);
    }

    if (!format) {
        printf("error, couldnt find valid format for alpha.\n");
    }

    { /* fill the shade-picture */ 
        Pixmap shade_pm = None;
        
        XRenderColor shade_color;
        XRenderPictureAttributes shade_attr;

        shade_color.alpha = 0xffff * (shade)/100;

        shade_attr.repeat = True;

        shade_pm = XCreatePixmap(dpy, src_pm, 1, 1, 8);
        shade_pic = XRenderCreatePicture(dpy, shade_pm, format, CPRepeat, &shade_attr);
        XRenderFillRectangle(dpy, PictOpSrc, shade_pic, &shade_color, 0, 0, 1, 1);
        XFreePixmap(dpy, shade_pm);
    }
            
    { /* blend all together */
        Picture src_pic;
        Picture dst_pic;
        
        format = XRenderFindVisualFormat(dpy, vis);

        src_pic = XRenderCreatePicture(dpy, src_pm, format, 0, 0);
        dst_pic = XRenderCreatePicture(dpy, dst_pm, format, 0, 0);

        XRenderComposite(dpy, PictOpOver, 
                         src_pic, shade_pic, dst_pic, 
                         0, 0, 0, 0, 0, 0, dim->width, dim->height);
        XRenderFreePicture(dpy, src_pic);
        XRenderFreePicture(dpy, dst_pic);
    }
    
    *bg_pm = dst_pm;
    XSetWindowBackgroundPixmap(dpy, win, dst_pm);
    XFreePixmap(dpy, src_pm);
#endif /* HAVE_XRENDER */
}

static void shape_window(Window win, Pixmap shape) {
#ifdef HAVE_XSHAPE
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, shape, ShapeSet);
#endif /* HAVE_XSHAPE */
}

static void main_loop() {
    
    struct timespec pause = {
        0, 40000000
    };

    struct timeval tval;
    struct timezone tz;
    
    Window tmp_win;

    int rootx;
    int rooty;
    int tmp_ignore;
    unsigned int tmp_ignore2;

    int i;

//printf("mainloop\n");

    if (show_buttons) {
        prev_ws.area_win = create_window(&prev_ws.area);
        next_ws.area_win = create_window(&next_ws.area);
    }

#ifdef HAVE_XSHAPE
    if (prev_ws.area_shape_pm)
        shape_window(prev_ws.area_win, prev_ws.area_shape_pm);
    if (next_ws.area_shape_pm)
        shape_window(next_ws.area_win, next_ws.area_shape_pm);
#endif /* HAVE_XSHAPE */

    for (;;) {
        
        XQueryPointer(dpy, root, &tmp_win, &tmp_win, 
                      &rootx, &rooty, 
                      &tmp_ignore, &tmp_ignore, &tmp_ignore2);

        gettimeofday(&tval, NULL);

        for (i = 0; i < 2; i++) {
            /* pointer is in buttons[i].area */
            if (rootx >= buttons[i]->area.x && rootx <= buttons[i]->area.x + buttons[i]->area.width &&
                rooty >= buttons[i]->area.y && rooty <= buttons[i]->area.y + buttons[i]->area.height) {

                if (buttons[i]->area_win && !buttons[i]->area_drawn) {
#ifdef HAVE_XRENDER
                    if (do_shading)
                        shade_window(buttons[i]->area_win, &buttons[i]->area_bg, &buttons[i]->area);
#endif /* HAVE_XRENDER */
                    XMapWindow(dpy, buttons[i]->area_win);
                    XRaiseWindow(dpy, buttons[i]->area_win);
                    buttons[i]->area_drawn = True;
                }

                if (buttons[i]->first_touch_area) {

                    buttons[i]->first_touch_area = False;
                    buttons[i]->first_time_over_area.tv_sec = tval.tv_sec;
                    buttons[i]->first_time_over_area.tv_usec = tval.tv_usec;

                }

		float td = ((tval.tv_sec - buttons[i]->first_time_over_area.tv_sec) * 1000) +
           		((tval.tv_usec - buttons[i]->first_time_over_area.tv_usec) / 1000);
		////printf("td: %f\n", td);
//printf("a: %f\n",activation_time);
                if (td >= activation_time)
                    change_ws_by_num(buttons[i]->change_ws_by);

            } else {
                buttons[i]->first_touch_area = True;
                if (buttons[i]->area_win) {
                    XUnmapWindow(dpy, buttons[i]->area_win);
                    buttons[i]->area_drawn = False;
                }
            }
        }
        
        nanosleep(&pause, NULL);
    }
}

int main(int argc, char* argv[]) {

    char* opt_area_next_ws = NULL;
    char* opt_area_prev_ws = NULL;
    char* opt_activation_time = NULL;
    char* opt_button_color = "white";
#ifdef HAVE_XRENDER
    char* opt_shade = NULL;
#endif /* HAVE_XRENDER */
#ifdef HAVE_XSHAPE
    char* opt_shape_prev_ws = NULL;
    char* opt_shape_next_ws = NULL;
#endif /* HAVE_XSHAPE */
    int i;

    // initialize buttons
    buttons[0] = &prev_ws;
    buttons[1] = &next_ws;
    for(i = 0; i < 2; i++) {
        buttons[i]->area_win = None;
        buttons[i]->area_bg = None;
        buttons[i]->area_drawn = False;
#ifdef HAVE_XSHAPE
        buttons[i]->area_shape_pm = None;
#endif /* HAVE_XSHAPE */
        buttons[i]->first_touch_area = True;
    }

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-h")) {

            display_usage();
            exit(0);
        } else if (!strcmp(argv[i], "-v")) {
            printf("%s %s (c) 2005 by Mathias Gumz.\n",
                    PROGRAM, VERSION);
            exit(0);
        } else if (!strcmp(argv[i], "-nws")) {
            if (++i < argc && strlen(argv[i])) {
                opt_area_next_ws = argv[i];   
            } else {
                ERRMSG("missing argument for -nws");
                exit(1);
            }
        } else if (!strcmp(argv[i], "-pws")) {
            if (++i < argc && strlen(argv[i])) {
                opt_area_prev_ws = argv[i];   
            } else {
                ERRMSG("missing argument for -pws");
                exit(1);
            }
        } else if (!strcmp(argv[i], "-t")) {
            if (++i < argc && strlen(argv[i])) {
                opt_activation_time = argv[i];   
            } else {
                ERRMSG("missing argument for -t");
                exit(1);
            }
        } else if (!strcmp(argv[i], "-b")) {
            show_buttons = True;
        } else if (!strcmp(argv[i], "-c")) {
            if (++i < argc && strlen(argv[i])) {
                opt_button_color = argv[i];
            } else {
                ERRMSG("missing argument for -c");
                exit(1);
            }
#ifdef HAVE_XRENDER
        } else if (!strcmp(argv[i], "-s")) {
            if (++i < argc && strlen(argv[i])) {
                opt_shade = argv[i];
            } else {
                ERRMSG("missing argument for -s");
                exit(1);
            }
#endif /* HAVE_XRENDER */
#ifdef HAVE_XSHAPE
        } else if (!strcmp(argv[i], "-spws")) {
            if (++i < argc && strlen(argv[i])) {
                opt_shape_prev_ws = argv[i];
            } else {
                ERRMSG("missing argument for -spws");
                exit(1);
            }
        } else if (!strcmp(argv[i], "-snws")) {
            if (++i < argc && strlen(argv[i])) {
                opt_shape_next_ws = argv[i];
            } else {
                ERRMSG("missing argument for -snws");
                exit(1);
            }
#endif /* HAVE_XSHAPE */
        } else {
            printf("opt \"%s\"\n",argv[i]);
            ERRMSG("unknown option, call -h. (%s)\n");
            exit(1);
        }
    }
        
    if (opt_activation_time) {
        activation_time = atof(opt_activation_time);
    }

#ifdef HAVE_XRENDER
    if (opt_shade) {
        int tmp = atoi(opt_shade);
        if (tmp < 1 || tmp > 99) {
            ERRMSG("wronge range for shading: [1,99] allowed.");
            exit(1);
        } else {
            shade = tmp;
            do_shading = True;
        }
    }
#endif /* HAVE_XRENDER */

    atexit(at_exit);
    signal(SIGTERM, at_signal);
    signal(SIGINT, at_signal);
    signal(SIGHUP, at_signal);

    if ((dpy = XOpenDisplay(NULL))) {
        root = DefaultRootWindow(dpy);
        scrn = DefaultScreen(dpy);

        if (!check_ewmh()) {
            ERRMSG("could'nt find wm supporting ewmh.");
            exit(1);
        }

        { /* get dimensions */
            XWindowAttributes attr;
            XGetWindowAttributes(dpy, root, &attr);
            width = attr.width;
            height = attr.height;
        }

        { /* default regions */
            prev_ws.area.x = 0;
            prev_ws.area.y = 0;
            prev_ws.area.width = 10;
            prev_ws.area.height = height;
            prev_ws.change_ws_by = -1;

            if (opt_area_prev_ws)
                update_region(opt_area_prev_ws, &prev_ws.area);

            next_ws.area.x = width - 10;
            next_ws.area.y = 0;
            next_ws.area.width = 10;
            next_ws.area.height = height;
            next_ws.change_ws_by = 1;

            if (opt_area_next_ws)
                update_region(opt_area_next_ws, &next_ws.area);
        }
#ifdef HAVE_XSHAPE
        { /* read in shape-files */
            unsigned int width = 0;
            unsigned int height = 0;
            Pixmap tmp_pm = None;
            int int_notused;

            if (opt_shape_prev_ws) {
                    if (XReadBitmapFile(dpy, root, opt_shape_prev_ws,
                                        &width, &height, &tmp_pm,
                                        &int_notused, &int_notused) == BitmapSuccess) {

                    prev_ws.area.width = width;
                    prev_ws.area.height = height;
                    prev_ws.area_shape_pm = tmp_pm;
                } else {
                    ERRMSG("couldnt load the bitmapfile\n");
                    printf("[%s]\n", opt_shape_prev_ws);
                }
            }

            if (opt_shape_next_ws) {
                    if (XReadBitmapFile(dpy, root, opt_shape_next_ws,
                                        &width, &height, &tmp_pm,
                                        &int_notused, &int_notused) == BitmapSuccess) {

                    next_ws.area.width = width;
                    next_ws.area.height = height;
                    next_ws.area_shape_pm = tmp_pm;
                } else {
                    ERRMSG("couldnt load the bitmapfile\n");
                    printf("[%s]\n", opt_shape_next_ws);
                }
            }

        }
#endif /* HAVE_XSHAPE */

        if (show_buttons) {
            XColor tmp;
    
            if((XAllocNamedColor(dpy, DefaultColormap(dpy, scrn), opt_button_color, &tmp, &button_color)) == 0)
                if ((XAllocNamedColor(dpy, DefaultColormap(dpy, scrn), "white", &tmp, &button_color)) == 0) {
                    ERRMSG("couldnt allocate shade_color.");
                    exit(1);
                }
        }
        
        main_loop();
    } else {
        ERRMSG("could'nt open display.");
        exit(1);
    }

    exit(0);
}

/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */

