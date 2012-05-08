
/*******************************************************************************/
/* Copyright (C) 2012 Jonathan Moore Liles                                     */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU Library General Public License as published      */
/* by the Free Software Foundation; either version 2 of the License, or (at    */
/* your option) any later version.                                             */
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


/* This class emulates the FLTK graphics drawing API using the Cairo
 * library. The advantage of doing this is that it provides
 * antialiasing and transparency on X11 for all FLTK widgets.
 *
 * The implementation inherits from the Xlib driver and attempts to
 * keep colors, clipping, and matrix transformations in sync, so that
 * calls to the base Xlib and Cairo routines can be mixed with the
 * expected results.
 * 
 * Fonts are still rendered using XFT. And since the FLTK image
 * drawing implementation already uses XRender, there would seem to be
 * little advantage to using Cairo for that either.
 *
 * Alpha values can be encoded into Fl_Color values by using the new
 * fl_color_add_alpha function--with the caveat that 100% transparency
 * (an Alpha of 0) is unsupported. This allows existing boxes and
 * widgets to be drawn with transparency simply by setting their
 * color() to one with an alpha value encoded.
 *
 * Antialiasing can be turned on and off with the new
 * fl_set_antialias() function. However, it should be noted that, even
 * with antialiasing disabled drawing complex lines and polygons is
 * still significantly slower with Cairo than with the base FLTK
 * routines.
 *
 */

#include <config.h>
#include <FL/Fl.H>

double fl_hxo = 0.0;
double fl_hyo = 0.5;
double fl_vxo = 0.5;
double fl_vyo = 0.0;
double fl_vho = 1.0;
double fl_hwo = 1.0;

#define HXO(n) ( n + fl_hxo )
#define HYO(n) ( n + fl_hyo )
#define VXO(n) ( n + fl_vxo )
#define VYO(n) ( n + fl_vyo )
#define VHO(n) ( n + fl_vho )
#define HWO(n) ( n + fl_hwo )

Fl_Color fl_color_add_alpha ( Fl_Color c, uchar alpha )
{
    if ( !( c & 0xFFFFFF00 ) )
    {
        /* this is an indexed color or black */
        if ( c & 0x000000FF )
        {
            /* this is an indexed color */
            uchar r,g,b;

            Fl::get_color( c, r, g, b );

            c = fl_rgb_color( r, g, b );
        }
        else
        {
            /* this is black */
            if ( 0 == alpha )
            {
                /* sorry, you can't have zero opacity because we don't
                 * have enough bits and it doesn't make much sense anyway */
                alpha = 255;
            }
            /* hack to represent black */
            c = 0x01010100;
        }
    }

    return ( c & 0xFFFFFF00 ) | alpha;
}

#if FLTK_USE_CAIRO

#include <FL/x.H>
#include <cairo/cairo.h>
#include <FL/Fl_Cairo.H>
#include <FL/fl_draw.H>
#include <math.h>
#include <FL/Fl_Device.H>

static double lw = 1;
static double hlw;
static cairo_antialias_t aa = CAIRO_ANTIALIAS_GRAY;

cairo_matrix_t Fl_Cairo_Graphics_Driver::m;
cairo_matrix_t  Fl_Cairo_Graphics_Driver::stack[FL_MATRIX_STACK_SIZE];
int  Fl_Cairo_Graphics_Driver::sptr;

static int awaiting_vertex = false;

#define cairo_set_antialias( cr, aa )

Fl_Cairo_Graphics_Driver::Fl_Cairo_Graphics_Driver ( )   : Fl_Xlib_Graphics_Driver ()
{
//    rstackptr = 0;
}

/* void Fl_Cairo_Graphics_Driver::set_current ( void ) */
/* { */
/*     Window root; */
    
/*     int x, y; */
/*     unsigned int w, h, bw, d; */

/*     XGetGeometry( fl_display, fl_window, &root, &x, &y, &w, &h, &bw, &d ); */

/*     fl_cairo_surface = cairo_create_surface( fl_gc, w, h ); */

/*     /\* FIXME: how are we going to free this? *\/ */
/*     fl_cairo_context = cairo_create( fl_cairo_surface ); */

/*     cairo_surface_destroy( fl_cairo_surface ); */
    
/*     fl_cairo_surface = 0; */
/* } */


#define set_cairo_matrix() \
{ \
    cairo_t *cr = fl_cairo_context; \
    if ( sptr ) \
         cairo_set_matrix( cr, &m );                     \
    else \
        cairo_identity_matrix( cr ); \
} 

#define restore_cairo_matrix() \
{ \
    cairo_t *cr = fl_cairo_context; \
    cairo_identity_matrix( cr ); \
}


void Fl_Cairo_Graphics_Driver::push_matrix ( void )
{
    cairo_t *cr = Fl::cairo_cc();

  cairo_get_matrix( cr, &m );

  if (sptr==FL_MATRIX_STACK_SIZE)
    Fl::error("fl_push_matrix(): matrix stack overflow.");
  else
    stack[sptr++] = m;
}

void Fl_Cairo_Graphics_Driver::pop_matrix ( void )
{
    cairo_t *cr = Fl::cairo_cc();
    
    if (sptr==0)
        Fl::error("fl_pop_matrix(): matrix stack underflow.");
    else 
        m = stack[--sptr];

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::translate ( double x, double y )
{
    cairo_t *cr = Fl::cairo_cc();
    cairo_matrix_translate( &m, x, y );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::scale ( double x, double y )
{
    cairo_t *cr = Fl::cairo_cc();
    cairo_matrix_scale( &m, x, y );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::rotate ( double a )
{
    cairo_t *cr = Fl::cairo_cc();
    cairo_matrix_rotate( &m, a * ( M_PI / 180.0 ) );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::mult_matrix ( double a, double b, double c, double d, double x, double y )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_matrix_t m2;

    cairo_matrix_init( &m2, a, b, c, d, x, y );

    cairo_matrix_multiply( &m, &m2, &m );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::line_style ( int style, int t, char* )
{
    cairo_t *cr = Fl::cairo_cc();

    if ( t == 0 || t == 1 )
    {
        double w1, w2;
        w1 = w2 = 1.0;
        cairo_device_to_user_distance (cr, &w1, &w2);
        lw = w1 > w2 ? w1 : w2;
    }
    else
        lw = t;

    hlw = lw / 2.0;
    
    cairo_set_line_width( cr, lw );

    cairo_set_line_cap( cr, CAIRO_LINE_CAP_BUTT );

    if ( style & FL_DASH )
    {
        const double dash[] = { lw, lw };
        int len  = sizeof(dash) / sizeof(dash[0]);

        cairo_set_dash( cr, dash, len, 0 );
    }
    else if ( style & FL_DOT )
    {
        const double dash[] = { lw, lw };
        int len  = sizeof(dash) / sizeof(dash[0]);
        
        cairo_set_dash( cr, dash, len, 0 );
        
        cairo_set_line_cap( cr, CAIRO_LINE_CAP_ROUND );
    }
    else
    {
        cairo_set_dash( cr, NULL, 0, 0 );
    }
}

void Fl_Cairo_Graphics_Driver::color ( Fl_Color c )
{
    uchar r,g,b;

    Fl_Xlib_Graphics_Driver::color( c );

    if ( ( c & 0x000000ff ) && ! ( c & 0xFFFFFF00 ) )
    {
        /* color is indexed, get the RGB value */
        Fl::get_color( c, r, g, b );

        /* FIXME: temp! */
        color( r, g, b );
       /* color( 255, 0, 0, 50 ); */
    }
    else
    {
        Fl::get_color( c & 0xFFFFFF00, r, g, b );

        /* lower 8 bits become alpha. */
        uchar a = c & 0x000000ff;

        if ( ! a )
            a = 255;

        /* /\* HACK to represent black *\/ */
        /* if ( ( c & 0xFFFFFF00 ) == 0x01010100 ) */
        /*     r = g = b = 0; */

        color( r, g, b, a );
    }
}

void Fl_Cairo_Graphics_Driver::color ( uchar r, uchar g, uchar b )
{
    cairo_t *cr = Fl::cairo_cc();

    Fl_Xlib_Graphics_Driver::color( r, g, b );

    if ( ! cr )
        return;
    
    cairo_set_source_rgb( cr, r /  255.0f, g / 255.0f, b / 255.0f );
}

void fl_set_antialias ( int v )
{
    cairo_t *cr = Fl::cairo_cc();

    switch ( v )
    {
        case FL_ANTIALIAS_DEFAULT:
            cairo_set_antialias( cr, aa = CAIRO_ANTIALIAS_DEFAULT );
            break;
        case FL_ANTIALIAS_ON:
            cairo_set_antialias( cr, aa = CAIRO_ANTIALIAS_GRAY );
            break;
        case FL_ANTIALIAS_OFF:
            cairo_set_antialias( cr, aa = CAIRO_ANTIALIAS_NONE );
            break;
    }
}


void Fl_Cairo_Graphics_Driver::color ( Fl_Color c, uchar a )
{
    uchar r,g,b;
    
    Fl::get_color( c, r, g, b );
    
    Fl_Xlib_Graphics_Driver::color( c );
    
    color( r, g, b, a);
}

void Fl_Cairo_Graphics_Driver::color (uchar r, uchar g, uchar b, uchar a  )
{
    cairo_t *cr = Fl::cairo_cc();

//    Fl_Xlib_Graphics_Driver::color( r, g, b );

    if ( ! cr )
        return;

    cairo_set_source_rgba( cr, r /  255.0f, g / 255.0f, b / 255.0f, a / 255.0f );
}

void Fl_Cairo_Graphics_Driver::circle( double x, double y, double r )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_arc( cr, x, y, r, 0, 2.0f * M_PI );
    
    restore_cairo_matrix();

    if ( what == POLYGON )
        cairo_fill( cr );
    else
        cairo_stroke( cr );

    set_cairo_matrix();
}

/* static void add_arc( int x, int y, int w, int h, double a1, double a2 ) */
/* { */
/*     cairo_t *cr = Fl::cairo_cc(); */

/*     /\* const double line_width = cairo_get_line_width( cr ); *\/ */

/*     const double cx = x + ( 0.5f * w ); */
/*     const double cy = y + ( 0.5f * h ); */

/*     /\* cairo_save( cr ); *\/ */
/*     /\* cairo_translate( cr, cx, cy ); *\/ */
/*     /\* cairo_scale( cr, w, h ); *\/ */

/*     /\* if ( a1 >  a2 ) *\/ */
/*     /\*     cairo_arc( cr, 0.0, 0.0, 0.5, a1 * ( -M_PI / 180.0 ), a2 * ( -M_PI / 180.0 )); *\/ */
/*     /\* else *\/ */
/*     /\*     cairo_arc_negative( cr, 0.0, 0.0, 0.5, a1 * ( -M_PI / 180.0 ), a2 * ( -M_PI / 180.0 )); *\/ */


/*     cairo_save( cr ); */
/*     cairo_translate( cr, cx, cy ); */
/*     cairo_scale( cr, w - 1, h - 1 ); */

/*     if ( a1 >  a2 ) */
/*         cairo_arc( cr, 0.0, 0.0, 0.5, a1 * ( -M_PI / 180.0 ), a2 * ( -M_PI / 180.0 )); */
/*     else */
/*         cairo_arc_negative( cr, 0.0, 0.0, 0.5, a1 * ( -M_PI / 180.0 ), a2 * ( -M_PI / 180.0 )); */

        
/*     cairo_restore( cr );  */
/* } */

static void add_arc( int x, int y, int w, int h, double a1, double a2, bool pie )
{
    cairo_t *cr = Fl::cairo_cc();

    double a1R = a1 * ( M_PI / 180.0 );
    double a2R = a2 * ( M_PI / 180.0 );

    double cx = x + 0.5 * w, cy = y + 0.5 * h;

    cairo_save( cr );

    cairo_translate( cr, cx, cy );
//    cairo_scale( cr, (w - lw), 0 - ((h - lw) ));
    cairo_scale( cr, w, 0 - h );

    if ( a1 > a2 )
        cairo_arc_negative( cr, 0.0, 0.0, 0.5, a1R, a2R );
    else
        cairo_arc( cr, 0.0, 0.0, 0.5, a1R, a2R );

    if ( pie )
    {
        cairo_line_to( cr, 0, 0 );
        cairo_close_path( cr );
    }

    cairo_restore( cr );
}

void Fl_Cairo_Graphics_Driver::arc( int x, int y, int w, int h, double a1, double a2 )
{
    cairo_t *cr = Fl::cairo_cc();

    add_arc( x, y, w, h, a1, a2, false );

    restore_cairo_matrix();

    cairo_stroke( cr );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::arc( double x, double y, double r, double a1, double a2 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_close_path( cr );

    cairo_arc( cr, x, y, r, a1 * ( -M_PI / 180.0 ), a2 * ( -M_PI / 180.0 ));
}

void Fl_Cairo_Graphics_Driver::pie( int x, int y, int w, int h, double a1, double a2 )
{
    cairo_t *cr = Fl::cairo_cc();

    add_arc( x, y, w, h, a1, a2, true );

    restore_cairo_matrix();

    cairo_fill( cr );

    set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::line( int x1, int y1, int x2, int y2 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );

//    restore_cairo_matrix();

    if ( x1 == x2 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
        /* vertical line */
            
        if ( y1 > y2 )
        {
            int t = y2;
            y2 = y1;
            y1 = t;
        }

        cairo_move_to( cr, VXO( x1 ), VYO( y1 ) );
        cairo_line_to( cr, VXO( x2 ), VHO( y2 ) );
    }
    else if ( y1 == y2 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
        /* horizontal line */
        cairo_move_to( cr, HXO( x1 ), HYO( y1 ) );
        cairo_line_to( cr, HWO( x2 ), HYO( y2 ) );        
    }
    else
    {
        /* diagonal line */
        cairo_move_to( cr, x1 , y1  );
        cairo_line_to( cr, x2 , y2  );
    }

    cairo_stroke( cr );

//    set_cairo_matrix();
        
    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::line( int x1, int y1, int x2, int y2, int x3, int y3 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );

    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }

//    restore_cairo_matrix();

    cairo_move_to( cr, x1 , y1  );
    cairo_line_to( cr, x2 , y2  );
    cairo_line_to( cr, x3 , y3  );
    
    cairo_stroke( cr );

//    set_cairo_matrix();

    cairo_set_antialias( cr, aa );
}


void Fl_Cairo_Graphics_Driver::rect ( int x, int y, int w, int h )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );

    /* cairo draws lines half inside and half outside of the path... */

    /* const double line_width = cairo_get_line_width( cr ); */
    /* const double o = line_width / 2.0;     */

    cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );

//    restore_cairo_matrix();
    
//    cairo_rectangle( cr, x + hlw, y + hlw, w - lw - 1, h - lw - 1); 
    cairo_rectangle( cr, VXO( x ), HYO( y  ), w - 1, h - 1 );


    cairo_stroke( cr );

//    set_cairo_matrix();

    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::rectf ( int x, int y, int w, int h )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );

//    restore_cairo_matrix();

    /* cairo fills the inside of the path... */
    cairo_rectangle( cr, x, y, w, h );

    cairo_fill( cr );

//    set_cairo_matrix();

    cairo_set_antialias( cr, aa );
}


void Fl_Cairo_Graphics_Driver::begin_line ( void )
{
    awaiting_vertex = true;
    what = LINE;
}

void Fl_Cairo_Graphics_Driver::begin_points ( void )
{
    awaiting_vertex = true;
    what = POINT_;
}

void Fl_Cairo_Graphics_Driver::begin_polygon ( void )
{
    awaiting_vertex = true;
    what = POLYGON;
}

void Fl_Cairo_Graphics_Driver::begin_loop ( void )
{
    awaiting_vertex = true;
    what = LOOP;
}

void Fl_Cairo_Graphics_Driver::begin_complex_polygon ( void )
{
    what = POLYGON;
    awaiting_vertex = true;
}

void Fl_Cairo_Graphics_Driver::end_line ( void )
{
   cairo_t *cr = Fl::cairo_cc();

   if ( lw <= 1 )
   {
       cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
   }

   cairo_set_line_width( cr, lw );

   restore_cairo_matrix();

   cairo_stroke( cr );

   set_cairo_matrix();

   cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::end_points ( void )
{
   cairo_t *cr = Fl::cairo_cc();

   cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );

   cairo_fill( cr );

   cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::end_loop ( void )
{
   cairo_t *cr = Fl::cairo_cc();
   cairo_close_path( cr );
   end_line();
}


void Fl_Cairo_Graphics_Driver::vertex ( double x, double y )
{
    cairo_t *cr = Fl::cairo_cc();

    if ( awaiting_vertex )
    {
        cairo_move_to( cr, x, y );
        awaiting_vertex = false;
    }
    else
        cairo_line_to( cr, x, y );
}

void Fl_Cairo_Graphics_Driver::gap ( void )
{
    awaiting_vertex = true;
}

void Fl_Cairo_Graphics_Driver::end_complex_polygon ( void )
{
   cairo_t *cr = Fl::cairo_cc();

   gap();

   cairo_close_path( cr );

   restore_cairo_matrix();
   
   cairo_fill( cr );

   set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::end_polygon ( void )
{
   cairo_t *cr = Fl::cairo_cc();

   cairo_close_path( cr );

   restore_cairo_matrix();

   cairo_fill( cr );

   set_cairo_matrix();
}

void Fl_Cairo_Graphics_Driver::curve( double x, double y, double x1, double y1, double x2, double y2, double x3, double y3 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_move_to( cr, x, y );
    cairo_curve_to( cr, x1, y1, x2, y2, x3, y3 );
}

void Fl_Cairo_Graphics_Driver::polygon ( int x, int y, int x1, int y1, int x2, int y2 )
{
    cairo_t *cr = Fl::cairo_cc();
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x1 , y1  );
    cairo_line_to( cr, x2 , y2  );
    cairo_close_path( cr );
    cairo_fill( cr );
}

void Fl_Cairo_Graphics_Driver::polygon ( int x, int y, int x1, int y1, int x2, int y2, int x3, int y3 )
{
    cairo_t *cr = Fl::cairo_cc();
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x1 , y1  );
    cairo_line_to( cr, x2 , y2  );
    cairo_line_to( cr, x3 , y3  );
    cairo_close_path( cr );
    cairo_fill( cr );
}

void Fl_Cairo_Graphics_Driver::loop ( int x, int y, int x1, int y1, int x2, int y2 )
{
    cairo_t *cr = Fl::cairo_cc();
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x1 , y1  );
    cairo_line_to( cr, x2 , y2  );
    cairo_close_path( cr );
    cairo_stroke( cr );
}

void Fl_Cairo_Graphics_Driver::loop ( int x, int y, int x1, int y1, int x2, int y2, int x3, int y3 )
{
    cairo_t *cr = Fl::cairo_cc();
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x1 , y1  );
    cairo_line_to( cr, x2 , y2  );
    cairo_line_to( cr, x3 , y3  );
    cairo_close_path( cr );
    cairo_stroke( cr );
}


void Fl_Cairo_Graphics_Driver::xyline ( int x, int y, int x1 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );


    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }

    cairo_move_to( cr, HXO( x ), HYO( y ) );
    cairo_line_to( cr, HWO( x1 ), HYO( y ) );
    
    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::xyline ( int x, int y, int x1, int y2 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );
    
    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }

    /* horizontal line */
    cairo_move_to( cr, HXO( x ) , HYO( y ) );
    cairo_line_to( cr, HWO( x1 ), HYO( y ) );
    /* then vertical line */
    cairo_line_to( cr, HWO( x1 ) , VYO( y2 ) );

    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );

}

void Fl_Cairo_Graphics_Driver::xyline ( int x, int y, int x1, int y2, int x3 )
{
    cairo_t *cr = Fl::cairo_cc();

    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x1 , y  );
    cairo_line_to( cr, x1 , y2  );
    cairo_line_to( cr, x3 , y2  );

    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::yxline ( int x, int y, int y1 )
{
    cairo_t *cr = Fl::cairo_cc();

    cairo_set_line_width( cr, lw );

    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }

    cairo_move_to( cr, VXO( x ), VHO( y ) );
    cairo_line_to( cr, VXO( x ), VYO( y1 ) );
    
    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::yxline ( int x, int y, int y1, int x2 )
{
    cairo_t *cr = Fl::cairo_cc();

    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }
    
    cairo_move_to( cr, x , y );
    cairo_line_to( cr, x, y1  );
    cairo_line_to( cr, x2, y1  );

    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );
}

void Fl_Cairo_Graphics_Driver::yxline ( int x, int y, int y1, int x2, int y3 )
{
    cairo_t *cr = Fl::cairo_cc();

    if ( lw <= 1 )
    {
        cairo_set_antialias( cr, CAIRO_ANTIALIAS_NONE );
    }
    
    cairo_move_to( cr, x , y  );
    cairo_line_to( cr, x , y1  );
    cairo_line_to( cr, x2 , y1  );
    cairo_line_to( cr, x2 , y3  );

    cairo_stroke( cr );

    cairo_set_antialias( cr, aa );
}

#else

void fl_set_antialias ( int v ) { }

#endif
