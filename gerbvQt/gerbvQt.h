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
//#define GERBVQT_MACRO_USE_TEMPIMAGE 1
#define GERBVQT_MACRO_CIRCLE_PRECISION 100

class gerbvQt {
	public:
		//See setDrawingMode
		enum drawingModeType {dm_CompositionModes, dm_TwoColors};
		
		gerbvQt();
		virtual ~gerbvQt();
		
		//Foreground and background colors
		void setForegroundColor(const QColor& _color) {fgColor = _color;}
		void setBackgroundColor(const QColor& _color) {bgColor = _color;}
		const QColor& foregroundColor(void) {return fgColor;}
		const QColor& backgroundColor(void) {return bgColor;}
		
		//Sets the drawing mode.
		//dm_CompositionModes uses SourceOver and Clear modes for foreground/background (inverse for negative)
		//dm_TwoColors uses two colors - foreground and background
		//Second mode is needed if you want to, for example, to draw on a Qt::Format_Mono QImage.
		//(The Qt::Format_Mono QImage does NOT support composition modes)
		void setDrawingMode(const drawingModeType& _dM) {dM = _dM;}
		const drawingModeType& drawingMode(void) {return dM;}
		
		//Main function
		void drawImageToQt(	QPaintDevice * device,
					const gerbv_image_t* gImage, 
					gerbv_user_transformation_t utransform, 
					const gerbv_render_info_t* renderInfo);
		
		//Returns the painter
		QPainter* getPainter(void) {return painter;}
		
		//Fill everything with the "background" color before drawing?
		//Be warned: if you are using the dm_CompositionMode drawing mode, then
		//the background may be erased!
		void setInitFill(bool _initFill) {startFill = _initFill;}
		bool initFill(void) {return startFill;}
		
		//Fill the full device or only the "bounding box" of the PCB?
		void setFillFullDevice(bool _fullyFill) {fullyFill = _fullyFill;}
		bool fillFullDevice(void) {return fullyFill;}
		
	private:
		QColor fgColor;
		QColor bgColor;
		QColor color;
		QPainter* painter;
		drawingModeType dM;
		
		bool fullyFill;
		bool startFill;
		
		void fillImage(const gerbv_image_t* gImage);
		void setNetstateTransform(QTransform* tr, gerbv_netstate_t *state);
		void drawNet(const gerbv_image_t* gImage, const gerbv_net_t* cNet);
		
		void generatePareaPolygon(QPainterPath& path, const gerbv_net_t* startNet);
		
		void drawLineCircle(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap);
		void drawLineRect(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap);
		void drawArcNet(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		
		void generateArcPath(QPainterPath& path, const gerbv_net_t* cNet);
		void generatePolygonPath(QPainterPath& path, const QPointF& center, double radius, int numPoints, double angle, bool ccw = true, double angleTo = 1.e10);
		
		void drawNetFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		void drawCircleFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawRectFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawOblongFlash(const QPointF& point, const gerbv_aperture_t* ap);
		void drawPolygonFlash(const QPointF& point, const gerbv_aperture_t* ap);
		

		
		
		//Macro
		void drawMacroFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap);
		void setMacroExposure(bool& var, double exposure);
		void generateMacroOutlinePath(QPainterPath& path, double* parameters);
		void generateMacroThermalPath(QPainterPath& path, double* parameters);
		
		//Set fg/bg
		void setMode(bool drawMode, QPainter* _painter = NULL);	//Sets the draw mode - true for "dark", false for "clear".
									//It either works with the bg/fg colors in dm_TwoColors mode
									//Or with the composition modes in dm_CompositionModes mode
		bool invertModes;
		
		//Composition modes and colors
		QPainter::CompositionMode darkMode;
		QPainter::CompositionMode clearMode;
		QColor uFgColor;
		QColor uBgColor;
};

#endif


