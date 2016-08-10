/*

    This file is part of gerbvQt.
    (c) Kurganov Alexander, 2016 me@sx107.ru

    gerbvQt is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with gerbvQt.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "gerbv.h"
#include "gerbvQt.h"
#include <iostream>
#include <cairo.h>

const double scaleFactor = 25.4 / 0.01; //One pixel is 0.01mm
const double border = 50; //50mm border

using namespace std;

int main(int argc, char** argv) {
	if(argc < 2) {cerr << "Usage: ./gerbvQtexample anyfile" << endl; return 1;}

	//Create a project and load one file in it
	gerbv_project_t *mainProject = gerbv_create_project();
	gerbv_open_layer_from_filename (mainProject, argv[1]);
	gerbv_image_t* image = mainProject->file[0]->image;
	gerbv_image_info_t* gInfo = mainProject->file[0]->image->info;

	int size_x = ceil((gInfo->max_x - gInfo->min_x)*scaleFactor)+border*2;
	int size_y = ceil((gInfo->max_y - gInfo->min_y)*scaleFactor)+border*2;

	gerbv_render_info_t RenderInfo;
	
	RenderInfo.renderType = GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY;
	
	RenderInfo.displayWidth = size_x;
	RenderInfo.displayHeight = size_y;
	
	//ScaleFactor
	RenderInfo.scaleFactorX = scaleFactor;
	RenderInfo.scaleFactorY = scaleFactor;
	
	//Shift
	RenderInfo.lowerLeftX = gInfo->min_x-border/scaleFactor;
	RenderInfo.lowerLeftY = gInfo->min_y-border/scaleFactor;
	
	QImage qtimage(size_x, size_y, QImage::Format_ARGB32);
	
	gerbvQt gqt;
	gqt.setColor(Qt::black);
	gqt.drawImageToQt(&qtimage, image, mainProject->file[0]->transform, &RenderInfo);
	
	QPainter painter(&qtimage);
	painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
	painter.fillRect(0, 0, size_x, size_y, Qt::white);
	
	qtimage.save("test.png");
	
	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size_x, size_y);
	cairo_t* cr = cairo_create (surface);
	cairo_set_source_rgba (cr, 1,1,1,1);
	cairo_paint(cr);
	
	GdkColor blackColor = {0, 0, 0, 0};
	mainProject->file[0]->color = blackColor;
	
	cairo_push_group (cr);
	gerbv_render_layer_to_cairo_target (cr, mainProject->file[0], &RenderInfo);
	cairo_pop_group_to_source (cr);
	cairo_paint_with_alpha (cr, 1);
	cairo_surface_flush(surface);
	
	cairo_surface_write_to_png(surface, "cairo.png");
	
	return 0;
}
