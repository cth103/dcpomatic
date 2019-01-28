#include "drawable.h"
#include <iostream>

#ifdef __WXMAC__
#include "OpenGL/gl.h"
#else
#include <GL/gl.h>
#endif

#include "wx/wx.h"

/*
 * This is a simple class built on top of OpenGL that manages drawing images in a higher-level and quicker way.
 */

DrawableThing::DrawableThing(Image* image_arg)
{

    x=0;
    y=0;
    hotspotX=0;
    hotspotY=0;
    angle=0;

    xscale=1;
    yscale=1;

    xflip=false;
    yflip=false;

    if(image_arg!=NULL) setImage(image_arg);
    else image=NULL;
}

void DrawableThing::setFlip(bool x, bool y)
{
    xflip=x;
    yflip=y;
}

void DrawableThing::setHotspot(int x, int y)
{
    hotspotX=x;
    hotspotY=y;
}

void DrawableThing::move(int x, int y)
{
    DrawableThing::x=x;
    DrawableThing::y=y;
}

void DrawableThing::scale(float x, float y)
{
    DrawableThing::xscale=x;
    DrawableThing::yscale=y;
}

void DrawableThing::scale(float k)
{
    DrawableThing::xscale=k;
    DrawableThing::yscale=k;
}

void DrawableThing::setImage(Image* image)
{
    DrawableThing::image=image;
}

void DrawableThing::rotate(int angle)
{
    DrawableThing::angle=angle;
}

void DrawableThing::render()
{
    assert(image!=NULL);

    glLoadIdentity();

    glTranslatef(x,y,0);

    if(xscale!=1 || yscale!=1)
	{
        glScalef(xscale, yscale, 1);
    }

    if(angle!=0)
	{
        glRotatef(angle, 0,0,1);
    }

    glBindTexture(GL_TEXTURE_2D, image->getID()[0] );

    glBegin(GL_QUADS);

    printf("%f %f %d %d\n", image->tex_coord_x, image->tex_coord_y, hotspotX, hotspotY);

    glTexCoord2f(xflip? image->tex_coord_x : 0, yflip? 0 : image->tex_coord_y);
    glVertex2f( -hotspotX, -hotspotY );

    glTexCoord2f(xflip? 0 : image->tex_coord_x, yflip? 0 : image->tex_coord_y);
    glVertex2f( image->width-hotspotX, -hotspotY );

    glTexCoord2f(xflip? 0 : image->tex_coord_x, yflip? image->tex_coord_y : 0);
    glVertex2f( image->width-hotspotX, image->height-hotspotY );

    glTexCoord2f(xflip? image->tex_coord_x : 0, yflip? image->tex_coord_y : 0);
    glVertex2f( -hotspotX, image->height-hotspotY );

    glEnd();

}
