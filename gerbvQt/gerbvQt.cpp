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



#include "gerbvQt.h"
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace std;

gerbvQt::gerbvQt() {
	bgColor = Qt::white;
	fgColor = Qt::black;
	color = Qt::black;
	dM = gerbvQt::dm_CompositionModes;
	painter = new QPainter;
	invertModes = false;
	fullyFill = false;
	startFill = true;
}

gerbvQt::~gerbvQt() {
	delete painter;
}

void gerbvQt::setMode(bool drawMode, QPainter* _painter) {
	if(_painter == NULL) {_painter = painter;}
	cout << "SetMode" << endl;
	switch(dM) {
		case dm_CompositionModes:
			if(drawMode xor invertModes) {_painter->setCompositionMode(QPainter::CompositionMode_SourceOver);}
			else {_painter->setCompositionMode(QPainter::CompositionMode_Clear);}
			color = fgColor;
			break;
		case dm_TwoColors:
			if(drawMode xor invertModes) {color = fgColor;}
			else {color = bgColor;}
			break;
	}
	if(_painter->pen().color() != color) {
		QPen q = _painter->pen();
		q.setColor(color);
		_painter->setPen(q);
	}
	if(_painter->brush().color() != color) {
		QBrush q = _painter->brush();
		q.setColor(color);
		_painter->setBrush(q);
	}
	cout << "SetMode end" << endl;
}

void gerbvQt::setNetstateTransform(QTransform* tr, gerbv_netstate_t *state) {
	tr->reset();
	tr->scale(state->scaleA, state->scaleB);
	tr->translate(state->offsetA, state->offsetB); //Shouldn't scale and transform be swapped here?
	switch(state->mirrorState) {
		case GERBV_MIRROR_STATE_FLIPA: tr->scale(-1, 1); break;
		case GERBV_MIRROR_STATE_FLIPB: tr->scale(1, -1); break;
		case GERBV_MIRROR_STATE_FLIPAB:	tr->scale(-1, -1); break;
		default: break;
	}
	
	if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		tr->rotate(270);
		tr->scale(1, -1);
	}
}

void gerbvQt::fillImage(const gerbv_image_t* gImage) {
	if(fullyFill) {
		painter->save();
		painter->resetTransform();
		painter->fillRect(0, 0, painter->device()->width(), painter->device()->height(), painter->brush());
		painter->restore();
	} else {
		painter->fillRect(QRectF(gImage->info->min_x,
					gImage->info->min_y,
					gImage->info->max_x - gImage->info->min_x,
					gImage->info->max_y - gImage->info->min_y),
					painter->brush());
	}
}

void gerbvQt::drawImageToQt(	QPaintDevice * device,
				const gerbv_image_t* gImage, 
				gerbv_user_transformation_t utransform, 
				const gerbv_render_info_t* renderInfo) {
	
	//Begin the painting
	painter->begin(device);
	
	//Set default brush and pen
	painter->setBrush(color);
	painter->setPen(color);
	
	//Reset transform
	painter->resetTransform();
	painter->setViewTransformEnabled(true);
	
	//Create the transform matrix
	QTransform globalTransform;
	
	//1. Revert the y (make it from the bottom)
	globalTransform.translate(0, renderInfo->displayHeight);
	globalTransform.scale(1, -1);
	
	//2. RenderInfo transformation from renderInfo - translation and scale
	globalTransform.scale(renderInfo->scaleFactorX, renderInfo->scaleFactorY);
	globalTransform.translate(-renderInfo->lowerLeftX, -renderInfo->lowerLeftY);
	
	//3. User transformation
	globalTransform.translate(utransform.translateX, utransform.translateY);
	globalTransform.scale(utransform.scaleX, utransform.scaleY);
	if(utransform.mirrorAroundX) {globalTransform.scale(-1, 1);}
	if(utransform.mirrorAroundY) {globalTransform.scale(1, -1);}
	globalTransform.rotate(utransform.rotation);
	
	//4. Image transform
	globalTransform.translate(gImage->info->imageJustifyOffsetActualA, gImage->info->imageJustifyOffsetActualB);
	globalTransform.translate(gImage->info->offsetA, gImage->info->offsetB);
	globalTransform.rotate(gImage->info->imageRotation);
	
	//Set the transform
	painter->setTransform(globalTransform);
	
	//Calculate the polarity
	bool invertImage = utransform.inverted;
	if (gImage->info->polarity == GERBV_POLARITY_NEGATIVE) {invertImage = !invertImage;}
	
	//Fill the image if we need it
	if(startFill) {
		if(invertImage) {
			//TODO: Make normal filling!
			setMode(true);
			this->fillImage(gImage);
			setMode(false);
		} else {
			setMode(false);
			this->fillImage(gImage);
			setMode(true);
		}
	}
	
	//Store and set the layer and state, so we can track when they change
	gerbv_netstate_t *oldState = nullptr;
	gerbv_layer_t *oldLayer = nullptr;
	
	//Create layer and state transform.
	QTransform layerTransform;
	QTransform stateTransform;
	
	//Main loop over all the nets
	for (gerbv_net_t* cNet = gImage->netlist; cNet; cNet = gerbv_image_return_next_renderable_object(cNet)) {
		//New layer
		if(cNet->layer != oldLayer) {
			//Apply all the transformations.
			layerTransform.reset();
			layerTransform.rotate(cNet->layer->rotation);
			
			//Set the transform.
			painter->resetTransform();
			painter->setTransform(globalTransform);
			painter->setTransform(layerTransform, true);
			
			invertModes = ((cNet->layer->polarity == GERBV_POLARITY_CLEAR) xor invertImage);
			
			//Draw the knockout area
			gerbv_knockout_t *ko = &(cNet->layer->knockout);
			if (ko->firstInstance == TRUE) {
				setMode(ko->polarity != GERBV_POLARITY_CLEAR);
				cout << "knockout: " << ko->width << " x " << ko->height << endl;
				painter->fillRect(QRectF(	ko->lowerLeftX - ko->border,
								ko->lowerLeftY - ko->border,
								ko->width + 2*ko->border,
								ko->height + 2*ko->border),
								painter->brush());
				
			}
			
			//Set the painter composition mode to darkMode
			setMode(true);
			
			//Add the state transform
			painter->setTransform(stateTransform, true);
		}
		
		//New state
		if(cNet->state != oldState) {
			setNetstateTransform(&stateTransform, cNet->state);
			painter->resetTransform();
			painter->setTransform(globalTransform);
			painter->setTransform(layerTransform, true);
			painter->setTransform(stateTransform, true);
		}
		
		oldState = cNet->state;
		oldLayer = cNet->layer;
		
		//if (cNet->aperture_state == GERBV_APERTURE_STATE_OFF) {continue;}
		QPainterPath* polygonPath = nullptr;
		if(cNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			//Generate the path.
			polygonPath = new QPainterPath();
			this->generatePareaPolygon(*polygonPath, cNet);
		}

		//S&R
		QTransform temp = painter->transform();
		gerbv_step_and_repeat_t *sr = &(cNet->layer->stepAndRepeat);
		for(int iX = 0; iX < sr->X; iX++) {
			for(int iY = 0; iY < sr->Y; iY++) {
				painter->resetTransform();
				painter->setTransform(temp, false);
				painter->setTransform(QTransform::fromTranslate(iX * sr->dist_X, iY * sr->dist_Y), true);
				if(polygonPath) {
					painter->fillPath(*polygonPath, painter->brush());
				} else {
					this->drawNet(gImage, cNet);
				}
			}
		}
		
		if(polygonPath) {delete polygonPath;}
	}
	painter->end();
}

void gerbvQt::drawNet(const gerbv_image_t* gImage, const gerbv_net_t* cNet) {

	QPointF start(cNet->start_x, cNet->start_y);
	QPointF stop(cNet->stop_x, cNet->stop_y);

	//Aperture is also unset on the paths (between PAREA_START and PAREA_END)
	gerbv_aperture_t* ap = gImage->aperture[cNet->aperture];
	if(ap == NULL) {return;}

	switch (cNet->aperture_state) {
		case GERBV_APERTURE_STATE_OFF:
			//Do nothing
			break;
		case GERBV_APERTURE_STATE_ON:			
			if(ap->type != GERBV_APTYPE_CIRCLE && ap->type != GERBV_APTYPE_RECTANGLE) {
				cerr << "Invalid instruction: for linear interpolation only circle and rectangle apertures are allowed." << endl;
				break;
			}	
			switch(cNet->interpolation) {
				case GERBV_INTERPOLATION_DELETED:
					//Do nothing
					break;
				case GERBV_INTERPOLATION_x10:
				case GERBV_INTERPOLATION_LINEARx01:
				case GERBV_INTERPOLATION_LINEARx001:
				case GERBV_INTERPOLATION_LINEARx1:
					if(ap->type == GERBV_APTYPE_CIRCLE) {
						drawLineCircle(start, stop, ap);
					} else if (ap->type == GERBV_APTYPE_RECTANGLE) {
						drawLineRect(start, stop, ap);
					} else {break;}
					break;
					
				case GERBV_INTERPOLATION_CW_CIRCULAR :
				case GERBV_INTERPOLATION_CCW_CIRCULAR :
					drawArcNet(cNet, ap);
					break;
				default:
					cerr << "Skipped interpolation type " << cNet->interpolation << endl;
					break;
			}
			break;
		case GERBV_APERTURE_STATE_FLASH:
			drawNetFlash(cNet, ap);
			break;
	}
}

void gerbvQt::drawLineCircle(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap) {
	//Parameters: diameter, hole diameter
	//Ignore the "Hole diameter" parameter[1]
	QPen pen;
	pen.setColor(color);
	pen.setWidthF(ap->parameter[0]);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter->setPen(pen);
	painter->drawLine(start, stop);
}

void gerbvQt::drawLineRect(QPointF& start, QPointF& stop, const gerbv_aperture_t* ap)  {
	//Parameters: width, height, hole diameter
	//Ignore the "Hole diameter" parameter[2]
	QPointF rectSize(ap->parameter[0]/2.0, ap->parameter[1]/2.0);
	if(start.x() > stop.x()) {rectSize.rx() *= -1;}
	if(start.y() > stop.y()) {rectSize.ry() *= -1;}
		
	QPointF points[6] = {
		QPointF(start.x() - rectSize.x(), start.y() - rectSize.y()),
		QPointF(start.x() - rectSize.x(), start.y() + rectSize.y()),
		QPointF(stop.x() - rectSize.x(), stop.y() + rectSize.y()),
		QPointF(stop.x() + rectSize.x(), stop.y() + rectSize.y()),
		QPointF(stop.x() + rectSize.x(), stop.y() - rectSize.y()),
		QPointF(start.x() + rectSize.x(), start.y() - rectSize.y())};
	painter->setPen(color);
	painter->setBrush(color);
	painter->drawPolygon(points, 6);
}

void gerbvQt::generateArcPath(QPainterPath& path, const gerbv_net_t* cNet) {
	double sign = (cNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR ? 1:-1);
	double ang1 = cNet->cirseg->angle1;
	double ang2 = cNet->cirseg->angle2;
	
	QPointF topleft(cNet->cirseg->cp_x, cNet->cirseg->cp_y);
	topleft -= QPointF(fabs(cNet->cirseg->width)/2.0, fabs(cNet->cirseg->height) / 2.0);
	QRectF arcRect(topleft, QSizeF(cNet->cirseg->width, cNet->cirseg->height));
	
	path.arcMoveTo(arcRect, sign*ang1);
	path.arcTo(arcRect, sign*ang1, sign*fabs(ang2-ang1));
}

void gerbvQt::drawArcNet(const gerbv_net_t* cNet, const gerbv_aperture_t* ap) {
	QPen pen;
	pen.setColor(color);
	pen.setWidthF(ap->parameter[0]);
	pen.setJoinStyle(Qt::RoundJoin);
	
	//TODO: Just using the FlatCap is NOT right. See gerber format specification: the aperture does NOT turn with the path!		
	if(ap->type == GERBV_APTYPE_CIRCLE) {
		pen.setCapStyle(Qt::RoundCap);
	} else {
		pen.setCapStyle(Qt::FlatCap);
	}	
	
	QPainterPath p;
	this->generateArcPath(p, cNet);
	painter->strokePath(p, pen);
}

void gerbvQt::drawNetFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap) {
	QPointF point(cNet->stop_x, cNet->stop_y);
	
	painter->setPen(color);
	painter->setBrush(color);
	
	switch(ap->type) {
		case GERBV_APTYPE_CIRCLE:
			drawCircleFlash(point, ap);
			break;
		case GERBV_APTYPE_RECTANGLE:
			drawRectFlash(point, ap);
			break;
		case GERBV_APTYPE_OVAL:
			drawOblongFlash(point, ap);
			break;
		case GERBV_APTYPE_POLYGON :
			drawPolygonFlash(point, ap);
			break;
		case GERBV_APTYPE_MACRO:
			drawMacroFlash(cNet, ap);
			break;
		default:
			cout << "Unknown aperture type: " << ap->type << endl;
			break;
	}
}

void gerbvQt::drawCircleFlash(const QPointF& point, const gerbv_aperture_t* ap) {
	QPainterPath f;
	f.addEllipse(point, ap->parameter[0] / 2.0, ap->parameter[0] / 2.0); // Main aperture shape
	f.addEllipse(point, ap->parameter[1] / 2.0, ap->parameter[1] / 2.0); // The hole
	painter->fillPath(f, painter->brush());
}

void gerbvQt::drawRectFlash(const QPointF& point, const gerbv_aperture_t* ap) {
	QPainterPath f;
	f.addRect(QRectF(point - QPointF(ap->parameter[0] / 2.0, ap->parameter[1] / 2.0), QSizeF(ap->parameter[0], ap->parameter[1])));
	f.addEllipse(point, ap->parameter[2] / 2.0, ap->parameter[2] / 2.0);
	painter->fillPath(f, painter->brush());
}

void gerbvQt::drawOblongFlash(const QPointF& point, const gerbv_aperture_t* ap) {
	QPainterPath f;
	QPointF hsize(ap->parameter[0] / 2.0, ap->parameter[1] / 2.0);
	QRectF rect(point - hsize, point + hsize);
	double rad = fmin(ap->parameter[0], ap->parameter[1]) / 2.0;
	
	f.addRoundedRect(rect, rad, rad);
	f.addEllipse(point, ap->parameter[2] / 2.0, ap->parameter[2] / 2.0);
	painter->fillPath(f, painter->brush());
}

void gerbvQt::generatePolygonPath(QPainterPath& path, const QPointF& center, double radius, int numPoints, double angle, bool ccw, double angleTo) {
	QPointF hSize(radius, radius);
	QRectF rect(center - hSize, center + hSize);
	
	double cAngle = angle;
	path.arcMoveTo(rect, cAngle);
	
	for(int i = 0; i < numPoints; i++) {
		if(cAngle > angleTo && angleTo != 1.e10 && ccw) {break;}
		if(cAngle < angleTo && angleTo != 1.e10 && !ccw) {break;}
		path.arcTo(rect, cAngle, 0.0);
		cAngle += (ccw?1.0:-1.0)*360.0 / double(numPoints);		
	}
	if(cAngle == 1.e10) {path.closeSubpath(); cout << "Closing the subpath." << endl;}
}

void gerbvQt::drawPolygonFlash(const QPointF& center, const gerbv_aperture_t* ap) {
	QPainterPath f;
	generatePolygonPath(f, center, ap->parameter[0] / 2.0, ap->parameter[1], ap->parameter[2]);
	f.addEllipse(center, ap->parameter[3] / 2.0, ap->parameter[3] / 2.0);
	painter->fillPath(f, painter->brush());
}

void gerbvQt::setMacroExposure(bool& var, double exposure) {
	if(exposure == 0.0) {var = false;}
	else if (exposure == 1.0) {var = true;}
	else {var = !var;}
}

void gerbvQt::drawMacroFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap) {
	//I can't decide which solution is better.
	
	//One solution creates a QPainterPath and uses the += and -= operators.
	//But they turn bezier curves (used to draw circles) into a line, so circles start to look like
	//heptagons. So, circles in this solution are drawn as polygons with GERBVQT_MACRO_CIRCLE_PRECISION sides.
	
	//The other solution is like the cairo_push_group function. A temporary QImage is created.
	//The GERBVQT_MACRO_USE_TEMPIMAGE switches to this solution.
	
	//Maybe there is another solution, which combines the advantages of these two?
	
	//TODO: Debug the moire and lines 20-22 primitives. I am not sure that they are working properly
	
	#ifdef GERBVQT_MACRO_USE_TEMPIMAGE
	QImage groupStorage(painter->device()->width(), painter->device()->height(), QImage::Format_ARGB32);
	groupStorage.fill(QColor(0, 0, 0, 0));
	QPainter groupPainter;
	groupPainter.begin(&groupStorage);
	groupPainter.setTransform(painter->transform());
	groupPainter.setPen(painter->pen());
	groupPainter.setBrush(painter->brush());
	groupPainter.setTransform(QTransform::fromTranslate(cNet->stop_x, cNet->stop_y), true);
	groupPainter.setRenderHints(painter->renderHints(), true);
	#else
	QPainterPath macroPath;
	macroPath.setFillRule(Qt::WindingFill);
	#endif
	
	bool cExp = true; //Exposure: true is "dark", false is "clear"
	
	for(gerbv_simplified_amacro_t* mac = ap->simplified; mac != NULL; mac = mac->next) {
		double* par = mac->parameter;
		QPainterPath apShape;
		QTransform apTransform;
		switch(mac->type) {
			case GERBV_APTYPE_MACRO_CIRCLE: {
				setMacroExposure(cExp, par[CIRCLE_EXPOSURE]);
				
				apTransform.rotate(par[4]);
				
				double rad = par[CIRCLE_DIAMETER] / 2.0;
				
				#ifdef GERBVQT_MACRO_USE_TEMPLATE
				apShape.addEllipse(QPointF(par[CIRCLE_CENTER_X], par[CIRCLE_CENTER_Y]), rad, rad);
				#else
				//Draw circle as a GERBVQT_MACRO_CIRCLE_PRECISION polygon.
				generatePolygonPath(apShape, QPointF(par[CIRCLE_CENTER_X], par[CIRCLE_CENTER_Y]), rad, GERBVQT_MACRO_CIRCLE_PRECISION, 0);
				#endif
			}
			break;
			case GERBV_APTYPE_MACRO_OUTLINE: {
				setMacroExposure(cExp, par[OUTLINE_EXPOSURE]);
				apTransform.rotate(par[2*int(par[OUTLINE_NUMBER_OF_POINTS]) + OUTLINE_ROTATION]);
				generateMacroOutlinePath(apShape, par);
			}
			break;
			case GERBV_APTYPE_MACRO_POLYGON: {
				setMacroExposure(cExp, par[POLYGON_EXPOSURE]);
				QPointF center(par[POLYGON_CENTER_X], par[POLYGON_CENTER_Y]);
				if(center != QPointF(0, 0) && par[POLYGON_ROTATION] != 0.0) {
					cerr << "Polygon rotation error: According to the Gerber format specification, to rotate the polygon it must be centered at (0, 0) position." << endl;
				}
				generatePolygonPath(apShape, center, par[POLYGON_DIAMETER] / 2.0, par[POLYGON_NUMBER_OF_POINTS], par[POLYGON_ROTATION]);
			}
			break;
			case GERBV_APTYPE_MACRO_LINE20: {
				setMacroExposure(cExp, par[LINE20_EXPOSURE]);

				QLineF vec(	par[LINE20_START_X], par[LINE20_START_Y],
						par[LINE20_END_X], par[LINE20_END_Y]);
						
				QLineF tvec; tvec.fromPolar(vec.angle() + 90.0, LINE20_LINE_WIDTH / 2.0);
				
				apShape.moveTo(vec.p1() - tvec.p2());
				apShape.lineTo(vec.p1() + tvec.p2());
				apShape.lineTo(vec.p2() + tvec.p2());
				apShape.lineTo(vec.p2() - tvec.p2());
				apShape.closeSubpath();
				
				apTransform.rotate(LINE20_ROTATION);
				
			}			
			break;
			case GERBV_APTYPE_MACRO_LINE21: {
				setMacroExposure(cExp, par[LINE21_EXPOSURE]);
				apTransform.rotate(LINE21_ROTATION);
				QPointF center(par[LINE21_CENTER_X], par[LINE21_CENTER_Y]);
				QPointF hSize(par[LINE21_WIDTH] / 2.0, par[LINE21_HEIGHT] / 2.0);
				apShape.addRect(QRectF(center - hSize, center + hSize));
			}			
			break;
			case GERBV_APTYPE_MACRO_LINE22: {
				setMacroExposure(cExp, par[LINE22_EXPOSURE]);
				apTransform.rotate(LINE22_ROTATION);
				apShape.addRect(par[LINE22_LOWER_LEFT_X],
						par[LINE22_LOWER_LEFT_Y],
						par[LINE22_WIDTH],
						par[LINE22_HEIGHT]);
			}
			break;
			case GERBV_APTYPE_MACRO_MOIRE: {
				if(par[MOIRE_CENTER_X] != 0 && par[MOIRE_CENTER_Y] != 0 && par[MOIRE_ROTATION] != 0) {
					cerr << "Thermal rotation error: According to the Gerber format specification, to rotate the thermal it must be centered at (0, 0) position." << endl;
				}
				//We will draw it later. It is too complex to be drawn on a single QPainterPath and I am not sure
				//that there will be no filling issues. But the rotation is applied here
				//Note: the moire primitive can only be rotated if it is located at (0, 0) -- only the crosshair rotates!
				apTransform.rotate(par[MOIRE_ROTATION]);
			}
			break;
			case GERBV_APTYPE_MACRO_THERMAL: {
				//Thermal exposure is always on.
				if(par[THERMAL_CENTER_X] != 0 && par[THERMAL_CENTER_Y] != 0 && par[THERMAL_ROTATION] != 0) {
					cerr << "Thermal rotation error: According to the Gerber format specification, to rotate the thermal it must be centered at (0, 0) position." << endl;
				}
				apTransform.rotate(par[THERMAL_ROTATION]); //It is easier to do that way
				generateMacroThermalPath(apShape, par);
			}
			break;
				
			default:
				cerr << "Unknown macro aptype " << mac->type << endl;
				break;
		}
		
		if(mac->type != GERBV_APTYPE_MACRO_MOIRE) {
			apShape = apTransform.map(apShape);
			#ifdef GERBVQT_MACRO_USE_TEMPIMAGE
			if(cExp || mac->type == GERBV_APTYPE_MACRO_THERMAL) {groupPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);}
			else {groupPainter.setCompositionMode(QPainter::CompositionMode_Clear);}
			groupPainter.fillPath(apShape, painter->brush());
			#else
			if(cExp || mac->type == GERBV_APTYPE_MACRO_THERMAL) {
				macroPath += apShape;
			}
			else {macroPath -= apShape;}
			#endif
		} else {
			//Draw a moire
			QPointF center(par[MOIRE_CENTER_X], par[MOIRE_CENTER_Y]);
			QPointF hSizeX(par[MOIRE_CROSSHAIR_LENGTH] / 2.0, par[MOIRE_CROSSHAIR_THICKNESS] / 2.0);
			QPointF hSizeY(par[MOIRE_CROSSHAIR_THICKNESS] / 2.0, par[MOIRE_CROSSHAIR_LENGTH] / 2.0);
			QPainterPath c1; c1.addRect(QRectF(center - hSizeX, center + hSizeX));
			QPainterPath c2; c2.addRect(QRectF(center - hSizeY, center + hSizeY));
			c1 = apTransform.map(c1);
			c2 = apTransform.map(c2);
			
			double ringOuter = par[MOIRE_OUTSIDE_DIAMETER] / 2.0;
			double ringThickness = par[MOIRE_CIRCLE_THICKNESS];
			double ringGap = ringThickness + par[MOIRE_GAP_WIDTH];
			
			#ifdef GERBVQT_MACRO_USE_TEMPIMAGE
				groupPainter.fillPath(c1, painter->brush());
				groupPainter.fillPath(c2, painter->brush());
				QPainterPath rings;
				
				for(int i = 0; i < int(par[MOIRE_NUMBER_OF_CIRCLES]); i++) {
					double outRadius = ringOuter - i*ringGap;
					double inRadius = outRadius - ringThickness;
					
					rings.addEllipse(center, outRadius, outRadius);
					rings.addEllipse(center, inRadius, inRadius);
				}
				groupPainter.fillPath(rings, painter->brush());
			#else
				macroPath += c1;
				macroPath += c2;
				
				for(int i = 0; i < int(par[MOIRE_NUMBER_OF_CIRCLES]); i++) {
					double outRadius = ringOuter - i*ringGap;
					double inRadius = outRadius - ringThickness;
					QPainterPath ring;
					generatePolygonPath(ring, center, outRadius, GERBVQT_MACRO_CIRCLE_PRECISION, 0);
					generatePolygonPath(ring, center, inRadius, GERBVQT_MACRO_CIRCLE_PRECISION, 0);
					macroPath += ring;
				}
			#endif
		}
	}
	
	#ifdef GERBVQT_MACRO_USE_TEMPIMAGE
	groupPainter.end();
	painter->save();
	painter->resetTransform();
	painter->drawImage(QPointF(0, 0), groupStorage);
	painter->restore();
	#else
	painter->save();
	painter->setTransform(QTransform::fromTranslate(cNet->stop_x, cNet->stop_y), true);
	painter->fillPath(macroPath, painter->brush());
	painter->restore();
	#endif
}

void gerbvQt::generateMacroOutlinePath(QPainterPath& path, double* par) {
	int numberOfPoints = (int) par[OUTLINE_NUMBER_OF_POINTS] + 1;				
	path.moveTo(par[OUTLINE_FIRST_X], par[OUTLINE_FIRST_Y]);
	
	for (int pI = 0; pI < numberOfPoints; pI++) {
		path.lineTo(	par[OUTLINE_FIRST_X + pI*2],
				par[OUTLINE_FIRST_Y + pI*2]);
	}
	
	path.closeSubpath();
}

void gerbvQt::generateMacroThermalPath(QPainterPath& path, double* par) {
	QPointF center(par[THERMAL_CENTER_X], par[THERMAL_CENTER_Y]);
	double r_outer = par[THERMAL_OUTSIDE_DIAMETER] / 2.0;
	double r_inner = par[THERMAL_INSIDE_DIAMETER] / 2.0;
	double halfgap = par[THERMAL_CROSSHAIR_THICKNESS] / 2.0;
	
	double ang_outer = atan2(halfgap, r_outer) * 360.0 / (2.0*M_PI);
	double ang_inner = atan2(halfgap, r_inner) * 360.0 / (2.0*M_PI);
	QRectF outerRect(center - QPointF(r_outer, r_outer), center + QPointF(r_outer, r_outer));
	QRectF innerRect(center - QPointF(r_inner, r_inner), center + QPointF(r_inner, r_inner));
	
	#ifdef GERBVQT_MACRO_USE_TEMPIMAGE

		for(int i = 0; i < 4; i++) {
			path.arcMoveTo(outerRect, ang_outer + i*90);
			path.arcTo(outerRect, ang_outer + i*90, 90 - ang_outer*2);
			path.arcTo(innerRect, (i+1)*90 - ang_inner, 0);
			path.arcTo(innerRect, (i+1)*90 - ang_inner, -(90 - ang_inner*2));
			path.closeSubpath();
		}
	#else
		for(int i = 0; i < 4; i++) {
			path.arcMoveTo(innerRect, ang_inner + i*90);
			path.arcTo(outerRect, ang_outer + i*90, 0);
			generatePolygonPath(path, center, r_outer, GERBVQT_MACRO_CIRCLE_PRECISION, ang_outer + i*90, true, (i+1)*90 - ang_outer);
			path.arcTo(outerRect, (i+1)*90 - ang_outer, 0);
			path.arcTo(innerRect, (i+1)*90 - ang_inner, 0);
			generatePolygonPath(path, center, r_inner, GERBVQT_MACRO_CIRCLE_PRECISION, (i+1)*90 - ang_inner, false, ang_inner + i*90);
			path.arcTo(innerRect, ang_inner + i*90, 0);
			path.arcTo(outerRect, ang_outer + i*90, 0);
			path.closeSubpath();
		}
	#endif
}

void gerbvQt::generatePareaPolygon(QPainterPath& path, const gerbv_net_t* startNet) {
	painter->setPen(color);
	painter->setBrush(color);

	bool firstPoint = true;
	gerbv_net_t* cNet = const_cast<gerbv_net_t*>(startNet); //Oh my god
	
	for(; cNet != NULL; cNet = cNet->next) {
		if(cNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {continue;}
		
		QPointF point(cNet->stop_x, cNet->stop_y);
		
		if(firstPoint) {
			path.moveTo(point);
			firstPoint = false;
			continue;
		}
		
		switch(cNet->interpolation) {
			case GERBV_INTERPOLATION_DELETED:
				//Seriously?
				break;
			case GERBV_INTERPOLATION_x10:
			case GERBV_INTERPOLATION_LINEARx01:
			case GERBV_INTERPOLATION_LINEARx001:
			case GERBV_INTERPOLATION_LINEARx1:
				path.lineTo(point);
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR:
			case GERBV_INTERPOLATION_CCW_CIRCULAR:
				path.lineTo(point); //Mhm. Is it right? O_o
				this->generateArcPath(path, cNet);
				break;
			case GERBV_INTERPOLATION_PAREA_END :
				path.closeSubpath();
				return;
			default:
				cerr << "Wrong interpolation type occured: " << cNet->interpolation << endl;
				break;
		}
	}
}
