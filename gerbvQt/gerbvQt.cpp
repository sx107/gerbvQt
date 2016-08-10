#include "gerbvQt.h"
#include <iostream>
#include <algorithm>

using namespace std;

gerbvQt::gerbvQt() {
	color = Qt::black;
	painter = new QPainter;
}

gerbvQt::~gerbvQt() {
	delete painter;
}

QPainter* gerbvQt::getPainter() {return painter;}

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
	painter->fillRect(QRectF(gImage->info->min_x,
				gImage->info->min_y,
				gImage->info->max_x - gImage->info->min_x,
				gImage->info->max_y - gImage->info->min_y),
				painter->brush());
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
	if(invertImage) {
		//TODO: Make normal filling!
		painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
		this->fillImage(gImage);
		painter->setCompositionMode(QPainter::CompositionMode_Clear);
		
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
			
			//Invert the polarity if we need to
			clearMode = QPainter::CompositionMode_Clear;
			darkMode = QPainter::CompositionMode_SourceOver;
			
			if((cNet->layer->polarity == GERBV_POLARITY_CLEAR) xor invertImage) {
				clearMode = QPainter::CompositionMode_SourceOver;
				darkMode = QPainter::CompositionMode_Clear;
			}
			
			
			//Draw the knockout area
			gerbv_knockout_t *ko = &(cNet->layer->knockout);
			if (ko->firstInstance == TRUE) {
				if(ko->polarity == GERBV_POLARITY_CLEAR) {painter->setCompositionMode(clearMode);}
				else {painter->setCompositionMode(darkMode);}
				cout << "knockout: " << ko->width << " x " << ko->height << endl;
				painter->fillRect(QRectF(	ko->lowerLeftX - ko->border,
								ko->lowerLeftY - ko->border,
								ko->width + 2*ko->border,
								ko->height + 2*ko->border),
								painter->brush());
				
			}
			
			//Set the painter composition mode to darkMode
			painter->setCompositionMode(darkMode);
			
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
			cout << "Polygon!" << endl;
			drawPolygonFlash(point, ap);
			break;
		case GERBV_APTYPE_MACRO:
			cout << "Macro!" << endl;
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
void gerbvQt::drawPolygonFlash(const QPointF& center, const gerbv_aperture_t* ap) {
	QPainterPath f;
	double radius = ap->parameter[0] / 2.0;
	QPointF hSize(radius, radius);
	QRectF rect(center - hSize, center + hSize);
	
	double cAngle = ap->parameter[2];
	f.arcMoveTo(rect, cAngle);

	for(int i = 0; i < ap->parameter[1]; i++) {
		f.arcTo(rect, cAngle, 0.0);
		cAngle += 360.0 / double(ap->parameter[2]);
	}
	f.closeSubpath();
	f.addEllipse(center, ap->parameter[3] / 2.0, ap->parameter[3] / 2.0);
	painter->fillPath(f, painter->brush());
}

void gerbvQt::drawMacroFlash(const gerbv_net_t* cNet, const gerbv_aperture_t* ap) {
	gerbv_simplified_amacro_t* mac = ap->simplified;
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
