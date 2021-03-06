/* xselectrectangle.cpp: Handles creating rectangles on the screen in pure X11.
 *
 * Copyright (C) 2014: Dalton Nell, Slop Contributors (https://github.com/naelstrof/slop/graphs/contributors).
 *
 * This file is part of Slop.
 *
 * Slop is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Slop is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Slop.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "xselectrectangle.hpp"

static Bool isDestroyNotify( Display* dpy, XEvent* ev, XPointer win ) {
    return ev->type == DestroyNotify && ev->xdestroywindow.window == *((Window*)win);
}

slop::XSelectRectangle::~XSelectRectangle() {
    if ( m_window == None ) {
        return;
    }
    // Try to erase the window before destroying it.
    XSetWindowBackground( xengine->m_display, m_window, 0 );
    XClearWindow( xengine->m_display, m_window );
    // Sleep for 0.1 seconds in hope that the rectangle was erased.
    usleep( 10000 );
    // Free up our color.
    XFreeColors( xengine->m_display, xengine->m_colormap, &m_color.pixel, 1, 0 );
    XDestroyWindow( xengine->m_display, m_window );
    XEvent event;
    // Block until the window is actually completely removed.
    XIfEvent( xengine->m_display, &event, &isDestroyNotify, (XPointer)&m_window );
    // Sleep for 0.1 seconds in hope that the screen actually cleared the window.
    usleep( 10000 );
}

slop::XSelectRectangle::XSelectRectangle( int sx, int sy, int ex, int ey, int border, bool highlight, float r, float g, float b, float a ) {
    m_x = std::min( sx, ex );
    m_y = std::min( sy, ey );
    m_width = std::max( sx, ex ) - m_x;
    m_height = std::max( sy, ey ) - m_y;
    m_border = border;
    m_window = None;
    m_highlight = highlight;

    // If we don't have a border, we don't exist, so just die.
    if ( m_border == 0 ) {
        return;
    }

    if ( m_highlight ) {
        m_border = 0;
    }

    m_color = convertColor( r, g, b );
    XSetWindowAttributes attributes;
    // Set up the window so it's our color 
    attributes.background_pixel = m_color.pixel;
    // Disable window decorations.
    attributes.override_redirect = True;
    // Make sure we know when we've been successfully destroyed later!
    attributes.event_mask = StructureNotifyMask;
    unsigned long valueMask = CWBackPixel | CWOverrideRedirect | CWEventMask;

    // Create the window
    m_window = XCreateWindow( xengine->m_display, xengine->m_root, 0, 0, xengine->getWidth(), xengine->getHeight(),
                              0, CopyFromParent, InputOutput,
                              CopyFromParent, valueMask, &attributes );


    if ( a < 1 ) {
        // Change the window opacity
        unsigned int cardinal_alpha = (unsigned int) (a * (unsigned int)-1) ;
        XChangeProperty( xengine->m_display, m_window, XInternAtom( xengine->m_display, "_NET_WM_WINDOW_OPACITY", 0),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&cardinal_alpha, 1 );
    }

    XClassHint classhints;
    char name[] = "slop";
    classhints.res_name = name;
    classhints.res_class = name;
    XSetClassHint( xengine->m_display, m_window, &classhints );

    // Now punch a hole into it so it looks like a selection rectangle, but only if we're not highlighting.
    if ( !m_highlight ) {
        XRectangle rects[4];
        // Left
        rects[0].x = m_x-m_border;
        rects[0].y = m_y-m_border;
        rects[0].width = m_border;
        rects[0].height = m_height+m_border*2;
        // Top
        rects[1].x = m_x;
        rects[1].y = m_y-m_border;
        rects[1].width = m_width+m_border;
        rects[1].height = m_border;
        // Right
        rects[2].x = m_x+m_width;
        rects[2].y = m_y-m_border;
        rects[2].width = m_border;
        rects[2].height = m_height+m_border*2;
        // Bottom
        rects[3].x = m_x;
        rects[3].y = m_y+m_height;
        rects[3].width = m_width+m_border;
        rects[3].height = m_border;
        XShapeCombineRectangles( xengine->m_display, m_window, ShapeBounding, 0, 0, rects, 4, ShapeSet, 0);
    } else {
        XRectangle rect;
        rect.x = m_x;
        rect.y = m_y;
        rect.width = m_width;
        rect.height = m_height;
        XShapeCombineRectangles( xengine->m_display, m_window, ShapeBounding, 0, 0, &rect, 1, ShapeSet, 0);
    }
    // Make it so all input falls through
    XRectangle rect;
    rect.x = rect.y = rect.width = rect.height = 0;
    XShapeCombineRectangles( xengine->m_display, m_window, ShapeInput, 0, 0, &rect, 1, ShapeSet, 0);
    XMapWindow( xengine->m_display, m_window );
}

void slop::XSelectRectangle::setGeo( int sx, int sy, int ex, int ey ) {
    int x = std::min( sx, ex );
    int y = std::min( sy, ey );
    int w = std::max( sx, ex ) - x;
    int h = std::max( sy, ey ) - y;

    // Only resize or move if we have to, because they're oddly expensive.
    m_x = x;
    m_y = y;
    m_width = w;
    m_height = h;
    if ( m_border > 0 ) {
        XRectangle rects[4];
        // Left
        rects[0].x = m_x-m_border;
        rects[0].y = m_y-m_border;
        rects[0].width = m_border;
        rects[0].height = m_height+m_border*2;
        // Top
        rects[1].x = m_x;
        rects[1].y = m_y-m_border;
        rects[1].width = m_width+m_border;
        rects[1].height = m_border;
        // Right
        rects[2].x = m_x+m_width;
        rects[2].y = m_y-m_border;
        rects[2].width = m_border;
        rects[2].height = m_height+m_border*2;
        // Bottom
        rects[3].x = m_x;
        rects[3].y = m_y+m_height;
        rects[3].width = m_width+m_border;
        rects[3].height = m_border;
        XShapeCombineRectangles( xengine->m_display, m_window, ShapeBounding, 0, 0, rects, 4, ShapeSet, 0);
    } else {
        XRectangle rect;
        rect.x = m_x;
        rect.y = m_y;
        rect.width = m_width;
        rect.height = m_height;
        XShapeCombineRectangles( xengine->m_display, m_window, ShapeBounding, 0, 0, &rect, 1, ShapeSet, 0);
    }
}

XColor slop::XSelectRectangle::convertColor( float r, float g, float b ) {
    // Convert float colors to shorts.
    short red   = short( floor( r * 65535.f ) );
    short green = short( floor( g * 65535.f ) );
    short blue  = short( floor( b * 65535.f ) );
    XColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    int err = XAllocColor( xengine->m_display, xengine->m_colormap, &color );
    if ( err == BadColor ) {
        fprintf( stderr, "Couldn't allocate color of value %f,%f,%f!\n", r, g, b );
    }
    return color;
}
