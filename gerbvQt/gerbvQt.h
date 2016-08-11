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


#ifndef GERBVQT
#include "gerbv.h"
#include <QImage>
#include <QPainter>

//See gerbvQt::drawMacroFlash(...)
#define GERBVQT_MACRO_USE_TEMPIMAGE 1
#define GERBVQT_MACRO_CIRCLE_PRECISION 100

class gerbvQt {
	public:
		gerbvQt();
		virtual ~gerbvQt();
		void setColor(const QColor& _color) {color = _color;}
		void drawImageToQt(	QPaintDevice * device,
					const gerbv_image_t* gImage, 
					gerbv_user_transformation_t utransform, 
					const gerbv_render_info_t* renderInfo);
		
		QPainter* getPainter(void);
	private:
		QColor color;
		QPainter* painter;
		
		void fillImage(const gerbv_image_t* gImage);
		void setNetstateTransform(QTransform* tr, gerbv_netstate_t *state);
		void drawNet(const gerbv_image_t* gImage, const gerbv_net_t* cNet);
		
		void generatePareaPolygon(QPainterPath& path, const gerbv_net_t* startNet);
		
		void drawLineCircle(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap);
		void drawLineRect(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap);
		void drawArcNet(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		
		void generateArcPath(QPainterPath& path, const gerbv_net_t* cNet);
		void generatePolygonPath(QPainterPath& path, const QPointF& center, double radius, int numPoints, double angle);
		
		void drawNetFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		void drawCircleFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawRectFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawOblongFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawPolygonFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawMacroFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		
		void setMacroExposure(bool& var, double exposure);
		
		//Composition modes
		QPainter::CompositionMode darkMode;
		QPainter::CompositionMode clearMode;
};

#endif


