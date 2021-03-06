#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <memory>

#include <Window.hpp>
#include <EGLHeaders.hpp>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#ifdef __arm__
    #include "bcm_host.h"
#else
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
    #include <X11/Xutil.h>
#endif

#define LINUX_SDL_TEST

#ifndef __arm__
// X11 related local variables
static Display * x_display = nullptr;
#endif

namespace {


#if defined __arm__
///
//  create_window() - RaspberryPi, direct surface(No X, Xlib)
//
//      This function initialized the display and window for EGL
//
EGLBoolean create_window(RPi::Context & context, const char *)
{
    static EGL_DISPMANX_WINDOW_T nativewindow;

    uint32_t display_width = 0;
    uint32_t display_height = 0;

    // create an EGL window surface, passing context width/height
    auto success = graphics_get_display_size(0 /* LCD */, 
        &display_width, &display_height);

    std::cout << "display_width :"  << display_width << std::endl;
    std::cout << "display_height :"  << display_height << std::endl;

    if(success < 0)
    {
      return EGL_FALSE;
    }

    display_width  = context.width;
    //display_height = context.height;

    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    dst_rect.x = context.x;
    dst_rect.y = context.y;
    dst_rect.width = display_width;
    dst_rect.height = display_height;
      
    src_rect.x = context.x;
    src_rect.y = context.y;
    src_rect.width = display_width << 16;
    src_rect.height = display_height << 16;   

    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0 /* LCD */);
    DISPMANX_UPDATE_HANDLE_T dispman_update  = vc_dispmanx_update_start(0);
         
    DISPMANX_ELEMENT_HANDLE_T dispman_element = 
        vc_dispmanx_element_add(dispman_update, dispman_display,
        0/*layer*/, &dst_rect, 0/*src*/, &src_rect, 
        DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, DISPMANX_NO_ROTATE/*transform*/);
      
    nativewindow.element = dispman_element;
    nativewindow.width = display_width;
    nativewindow.height = display_height;
    vc_dispmanx_update_submit_sync(dispman_update);

    context.eglWindow = &nativewindow;

    return EGL_TRUE;
}

///
//  user_interrupt()
//
//      Reads from X11 event loop and interrupt program if there is a keypress, or
//      window close action.
//
GLboolean user_interrupt(RPi::Context &)
{
    //GLboolean userinterrupt = GL_FALSE;
    //return userinterrupt;
    
    // Ctrl-C for now to stop
    
    return GL_FALSE;
}

#else
///
//  create_window()
//
//      This function initialized the native X11 display and window for EGL
//
EGLBoolean create_window(RPi::Context & context, const char *title)
{
    // Get an handle of the display of the X server
    x_display = XOpenDisplay(nullptr);

    if(x_display == nullptr)
    {
        return EGL_FALSE;
    }

    // Get the default root window
    Window root = DefaultRootWindow(x_display);

    // Set the event mask of the window
    XSetWindowAttributes swa;
    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

    // Create the new window
    Window window = XCreateWindow(
                x_display, root,
                context.x, context.y, context.width, context.height, 0,
                CopyFromParent, InputOutput,
                CopyFromParent, CWEventMask,
                &swa);

    // Set window's attributes
    XSetWindowAttributes xattr;
    xattr.override_redirect = false;
    XChangeWindowAttributes(x_display, window, CWOverrideRedirect, &xattr);

    // Set window manager hints
    // -> the app relies on the WM to get keyboard input
    XWMHints hints;
    hints.input = true;
    hints.flags = InputHint;
    XSetWMHints(x_display, window, &hints);

    // Set window's title
    XStoreName(x_display, window, title);

    // Make the window visible on the screen
    XMapWindow(x_display, window);

    // Get identifiers for the provided atom name strings
    Atom wm_state;
    wm_state = XInternAtom(x_display, "_NET_WM_STATE", false);

    XEvent xev;
    std::memset(&xev, 0, sizeof(xev));
    xev.type                 = ClientMessage;
    xev.xclient.window       = window;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = 1;
    xev.xclient.data.l[1]    = false;
    XSendEvent(x_display, DefaultRootWindow(x_display), false, 
        SubstructureNotifyMask, &xev);

    // Set the eglWindow attribute of the current context to
    // the newly created window
    context.eglWindow = (EGLNativeWindowType) window;

    return EGL_TRUE;
}


///
//  user_interrupt()
//
//      Reads from X11 event loop and interrupt program if there is a keypress, or
//      window close action.
//
GLboolean user_interrupt(RPi::Context & context)
{
    GLboolean userinterrupt = EGL_FALSE;

    //#ifndef LINUX_SDL_TEST
    XEvent xev;
    KeySym key;
    char text;

    //Pump all messages from X server. Keypresses are directed to keyfunc(if defined)
    while(XPending(x_display))
    {
        XNextEvent(x_display, &xev);
        if(xev.type == KeyPress)
        {
            if(XLookupString(&xev.xkey, &text, 1, &key, 0) == 1)
            {
                std::cout << "Key pressed : " << static_cast<unsigned int>(text) << std::endl;

                static auto const key_equal = [](char text, unsigned int key)
                {
                    return static_cast<unsigned int>(text) == key;
                };

                if(context.keyFunc != nullptr)
                    context.keyFunc(context, text, 0, 0);

                if(key_equal(text, 27))
                    userinterrupt = EGL_TRUE;
            }
        }
        if(xev.type == MotionNotify)
        {
            auto const & mev = xev.xmotion;
            std::cout << "Mouse moved : " << mev.x << " | " << mev.y << " || " << mev.x_root << " | " << mev.y_root << std::endl;
        }
        if(xev.type == DestroyNotify)
            userinterrupt = EGL_TRUE;
    }
    //#endif

    return userinterrupt;
}
#endif


#if defined __arm__ || defined LINUX_SDL_TEST

// Load a font
//std::shared_ptr<TTF_Font> s_font = nullptr;
TTF_Font * s_font = nullptr;

void load_font(std::string const & fontPath)
{
    s_font = TTF_OpenFont(fontPath.c_str(), 24);
    if (s_font == nullptr)
    {
        std::cerr << "TTF_OpenFont() Failed: " << TTF_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        exit(1);
    }
}

// Write text to surface
SDL_Surface * s_text = nullptr;
SDL_Color const s_text_color = {255, 255, 255, 255};
SDL_Color const s_bg_color = {0, 0, 0, 255};
SDL_Surface * s_screen = nullptr;
SDL_Surface * s_clear_surface = nullptr;

void display_text(std::string const & text)
{
    if(s_clear_surface == nullptr)
    {
        s_clear_surface = SDL_CreateRGBSurface( SDL_HWSURFACE, s_screen->w, s_screen->h * 0.2, 32, 0, 0, 0, 0); 
    }

    if(s_text != nullptr)
    {
        delete s_text;
    }

    //s_text = TTF_RenderText_Solid(s_font, text.c_str(), s_text_color);
    s_text = TTF_RenderText_Shaded(s_font, text.c_str(), s_text_color, s_bg_color);

    if(s_text == nullptr)
    {
        std::cerr << "TTF_RenderText_Solid() Failed: " << TTF_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        exit(1);
    }

    assert(s_screen != nullptr);
    assert(s_clear_surface != nullptr);

    // Clear the screen
    if(SDL_BlitSurface(s_clear_surface, nullptr, s_screen, nullptr) != 0)
    {
        std::cerr << "SDL_BlitSurface() Failed: " << SDL_GetError() << std::endl;
    }

    // Apply the text to the display
    if (SDL_BlitSurface(s_text, nullptr, s_screen, nullptr) != 0)
    {
        std::cerr << "SDL_BlitSurface() Failed: " << SDL_GetError() << std::endl;
    }

    SDL_Flip(s_screen);
}

#else

void load_font(std::string const &)
{

}

void display_text(std::string const &)
{

}

#endif



EGLBoolean create_egl_context(RPi::Context & rpiContext, EGLint const attribList[])
{
    // Obtains the EGL display connection for the given native display 
    EGLDisplay display;

    //#ifdef __arm__
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    //#else
    //display = eglGetDisplay(static_cast<EGLNativeDisplayType>(x_display));
    //#endif
    
    std::cout << "Get display OK" << std::endl;

    if(display == EGL_NO_DISPLAY)
    {
        std::cerr << "Failed to get display" << std::endl;
        return EGL_FALSE;
    }

    // Initialize EGL
    if(eglInitialize(display, nullptr, nullptr) == EGL_FALSE)
    {
        std::cerr << "Failed to initialize EGL" << std::endl;
        return EGL_FALSE;
    }

    std::cout << "Initialize display OK" << std::endl;

    // Get configs
    EGLint nbConfigs;
    if(eglGetConfigs(display, nullptr, 0, &nbConfigs) == EGL_FALSE)
    {
        std::cerr << "Failed to get configs" << std::endl;
        return EGL_FALSE;
    }

    std::cout << "Get config OK" << std::endl;

    // Choose config
     EGLConfig config;
    if(eglChooseConfig(display, attribList, &config, 1, &nbConfigs) == EGL_FALSE)
    {
        std::cerr << "Failed to choose configs" << std::endl;
        return EGL_FALSE;
    }

    std::cout << "Choose config OK" << std::endl;

    // Defines the current rendering API
    if(eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        std::cerr << "Failed to bind EGL API" << std::endl; return EGL_FALSE;
    }

    std::cout << "Bind API OK" << std::endl;

    // Create a surface
    #ifdef __arm__
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    #else
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
    #endif

    EGLSurface surface = eglCreateWindowSurface(display, config,
        (EGLNativeWindowType) rpiContext.eglWindow, nullptr);

    if(surface == EGL_NO_SURFACE)
    {
        std::cerr << "Failed to create surface" << std::endl;
        return EGL_FALSE;
    }

    std::cout << "Create surface OK" << std::endl;


    // Create an EGL context
     EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
     if(context == EGL_NO_CONTEXT)
     {
        std::cerr << "Failed to create context" << std::endl;
         return EGL_FALSE;
     }

    std::cout << "Create context OK" << std::endl;

     // Make the context current
     if(eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
     {
        std::cerr << "Failed to make context current" << std::endl;
         return EGL_FALSE;
     }

    std::cout << "Make context current OK" << std::endl;

    rpiContext.eglDisplay = display;
    rpiContext.eglSurface = surface;
    rpiContext.eglContext = context;

    return EGL_TRUE;
}

}

namespace RPi {

Window::Window(Context & context, char const * title,
    int x, int y, int width, int height, WindowFlags flags):
    m_width(width), m_height(height), m_context(context)
{
    context.x = x;
    context.y = y;
    context.width  = m_width;
    context.height = m_height;

    #if defined __arm__ || defined LINUX_SDL_TEST
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed" << std::endl;
    }

    auto info = SDL_GetVideoInfo();
    m_width = info->current_w;
    m_height = info->current_h;

    std::cout << "screenWidth = " << m_width << std::endl;
    std::cout << "screenHeight = " << m_height << std::endl;

    std::ostringstream windowSize;

    windowSize << "SDL_VIDEO_WINDOW_POS=" 
               << context.x << "," << context.y;

    putenv(strdup(windowSize.str().c_str())); 
    s_screen = SDL_SetVideoMode(m_width / 2, m_height, 8, SDL_SWSURFACE);

    //s_screen = SDL_SetVideoMode(1366, 768, 8, SDL_HWSURFACE | SDL_FULLSCREEN);
    //s_screen = SDL_SetVideoMode(0, 0, 0, SDL_SWSURFACE | SDL_FULLSCREEN);

    SDL_WM_SetCaption(title, nullptr);

    if(s_screen == nullptr)
    {
        std::cerr << "SDL_SetVideoMode failed" << std::endl;
    }

    m_width  = s_screen->w;
    m_height = s_screen->h;

    context.width  = m_width;
    context.height = m_height;

     //Initialize SDL_ttf library
    if(TTF_Init() != 0)
    {
        std::cerr << "TTF_Init() Failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        exit(1);
    }
    #endif

    load_font("res/fonts/FreeSans.ttf");

    // Creates the window
    if(create_window(context, title) == EGL_FALSE)
    {
        std::cerr << "Failed to create window" << std::endl;
    }

    // EGL context attributes
    EGLint const attribList[] =
    {
        EGL_RED_SIZE,       5,
        EGL_GREEN_SIZE,     6,
        EGL_BLUE_SIZE,      5,
        EGL_ALPHA_SIZE,     (flags & WINDOW_ALPHA)   ? 8 : EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     (flags & WINDOW_DEPTH)   ? 8 : EGL_DONT_CARE,
        EGL_STENCIL_SIZE,   (flags & WINDOW_STENCIL) ? 8 : EGL_DONT_CARE,
        EGL_SAMPLE_BUFFERS, (flags & WINDOW_ALPHA)   ? 1 : 0,
        //EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    // Creates EGL context
    if(create_egl_context(context, attribList) == EGL_FALSE)
    {
        std::cerr << "Failed to create egl context" << std::endl;
    }
}

Window::~Window()
{
    std::cout << "Window destroyed" << std::endl;
}

Context & Window::getContext() const
{
    return m_context;
}

int Window::getWidth() const
{
    return m_width;
}

int Window::getHeight() const
{
    return m_height;
}

void Window::clear() const
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::display() const
{
    eglSwapBuffers(m_context.eglDisplay, m_context.eglSurface);
}

void Window::displayText(std::string const & text) const
{
    glDisable(GL_DEPTH_TEST);
    display_text(text);
    glEnable(GL_DEPTH_TEST);
}

void Window::showMousePointer(bool show) const
{
    if(show)
        SDL_ShowCursor(SDL_ENABLE);

    else
        SDL_ShowCursor(SDL_DISABLE);
}

void Window::grabMousePointer(bool grab) const
{
    (void) grab;
    //if(grab)
        //SDL_SetRelativeMouseMode(SDL_TRUE);

    //else
        //SDL_SetRelativeMouseMode(SDL_FALSE);
}


void Window::init() const
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glEnable(GL_DEPTH_TEST);
}

bool Window::userInterrupt()
{
    return user_interrupt(m_context) == EGL_TRUE;
}

}

