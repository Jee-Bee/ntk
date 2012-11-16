
/*******************************************************************************/
/* Copyright (C) 2012 Jonathan Moore Liles                                     */
/* Copyright (C) 2001-2005 by Colin Jones                                      */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/

#include <cairo/cairo.h>

#include <FL/Fl.H>
#include <FL/Fl_Cairo.H>
#include <FL/x.H>
#include <FL/fl_draw.H>

#include "FL/Fl_Theme.H"
#include <math.h>

static const int DX = 1;

static void cairo_color(Fl_Color c)
{
    cairo_t *cr = Fl::cairo_cc();

    uchar r,g,b;
    
    Fl_Color bc = Fl::draw_box_active() ? c : fl_inactive( c );

    fl_color( bc );
    
    Fl::get_color( bc, r, g, b );

    cairo_set_source_rgb( cr, r / 255.0, g / 255.0, b / 255.0 );
}

static void rect_path ( int x, int y, int w, int h, double radius )
{
    cairo_t *cr = Fl::cairo_cc();

    double degrees = M_PI / 180.0;
    
//    x += 2;  y += 2; w -= 4; h -= 4;

    x += DX;  y += DX; w -= DX*2; h -= DX*2;

    cairo_new_sub_path (cr);
    cairo_arc (cr, x + w - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, x + w - radius, y + h - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, x + radius, y + h - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);
}

static void draw_rect(int x, int y, int w, int h, Fl_Color bc, double radius = 2 )
{
    cairo_t *cr = Fl::cairo_cc();

    rect_path( x, y, w, h, radius );
    
       
    cairo_color( bc );

    cairo_set_line_width (cr, DX + 0.5);
    cairo_stroke (cr);
    cairo_set_line_width (cr, 1);
}

static void draw_rectf(int x, int y, int w, int h, Fl_Color bc, double radius = 2 )
{
    /* // Draw the outline around the perimeter of the box */
    /* fl_color(fl_color_average(FL_BLACK, FL_BACKGROUND_COLOR, .1)); */

    cairo_t *cr = Fl::cairo_cc();

    rect_path( x, y, w, h, radius );

    cairo_color( bc );
    cairo_fill (cr);

//    cairo_set_line_width (cr, 1);
}

static void border_box(int x, int y, int w, int h, Fl_Color bc)
{
    draw_rectf( x, y, w, h, FL_BACKGROUND_COLOR );
    
    draw_rect( x, y, w, h, bc );
}

static void shade_rect_up(int x, int y, int w, int h, Fl_Color bc)
{
    draw_rectf( x, y, w, h, fl_darker( bc ) );
}

static void frame_rect_up(int x, int y, int w, int h, Fl_Color bc)
{
    draw_rect( x,y,w,h, fl_darker( bc ));
}

static void frame_rect_down(int x, int y, int w, int h, Fl_Color bc)
{
    draw_rect( x,y,w,h, bc );
}

static void shade_rect_down(int x, int y, int w, int h, Fl_Color bc)
{
    draw_rectf( x, y, w, h, bc );
}

static void up_frame(int x, int y, int w, int h, Fl_Color c)
{
    frame_rect_up(x, y, w, h, fl_darker(c));
}

static void up_box(int x, int y, int w, int h, Fl_Color c)
{
    shade_rect_up(x, y, w, h, c);
}

static void down_frame(int x, int y, int w, int h, Fl_Color c)
{
    frame_rect_down(x, y, w, h, c);
}

static void down_box(int x, int y, int w, int h, Fl_Color c)
{
    shade_rect_down(x, y, w, h,  c);
}

static void
init_theme ( void )
{
    Fl::set_boxtype(  FL_UP_BOX,         up_box,             DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_DOWN_BOX,       down_box,           DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_THIN_UP_BOX,    up_box,             DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_THIN_DOWN_BOX,  down_box,           DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_UP_FRAME,       up_frame,           DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_DOWN_FRAME,     down_frame,         DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_ROUND_UP_BOX,   up_box,             DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_ROUND_DOWN_BOX, down_box,           DX,DX,DX*2,DX*2  );
    Fl::set_boxtype(  FL_BORDER_BOX,     border_box,         DX,DX,DX*2,DX*2  );
}

void
init_vector_theme ( void )
{
    Fl_Theme *t = new Fl_Theme( "Vector", "Simple vector theme based on Cairo", "Jonathan Moore Liles", init_theme );

    Fl_Theme::add( t );
}

