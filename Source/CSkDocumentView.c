/*
    File:       CSkDocumentView.c
        
    Contains:	Implementation of the scrollable document view.

    Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                ("Apple") in consideration of your agreement to the following terms, and your
                use, installation, modification or redistribution of this Apple software
                constitutes acceptance of these terms.  If you do not agree with these terms,
                please do not use, install, modify or redistribute this Apple software.

                In consideration of your agreement to abide by the following terms, and subject
                to these terms, Apple grants you a personal, non-exclusive license, under Appleâ€™s
                copyrights in this original Apple software (the "Apple Software"), to use,
                reproduce, modify and redistribute the Apple Software, with or without
                modifications, in source and/or binary forms; provided that if you redistribute
                the Apple Software in its entirety and without modifications, you must retain
                this notice and the following text and disclaimers in all such redistributions of
                the Apple Software.  Neither the name, trademarks, service marks or logos of
                Apple Computer, Inc. may be used to endorse or promote products derived from the
                Apple Software without specific prior written permission from Apple.  Except as
                expressly stated in this notice, no other rights or licenses, express or implied,
                are granted by Apple herein, including but not limited to any patent rights that
                may be infringed by your derivative works or by other works in which the Apple
                Software may be incorporated.

                The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
                WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
                WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
                PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
                COMBINATION WITH YOUR PRODUCTS.

                IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
                CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
                GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
                ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
                OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
                (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
                ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Copyright © 2005 Apple Computer, Inc., All Rights Reserved
*/

#include "CSkConstants.h"
#include "CSkUtils.h"
#include "CSkToolPalette.h" // needed for CSkToolPaletteGetTool

#include "CSkDocumentView.h"
#include "CSkDocStorage.h"

#define kCSkDocViewClassID	CFSTR( "com.apple.sample.cskdocview" )

//------------------------------------------------------------------------------------------------
// Here are the different mouse tracking scenarios
enum TrackingMode {
    eDragSelection	    = 0,
    eMoveSelection	    = 1,
    eDuplicateSelection	    = 2,
    eResizeViaGrabber	    = 3,
    eCreateObject	    = 4,
    eChangedSelectState	    = 5
};
typedef int TrackingMode;

struct CanvasData {
    HIViewRef		theView;
    HIPoint		scrollPosition;		// defined by scrolling
    HISize		canvasSize;		// includes background around document page
    float		zoomFactor;		// default = 1.0
    Boolean		mouseUpTracking;	// for polygons and bezier curves
    
    // mouse-tracking data
    
    int			hitGrabber;		// click count for hit-testing
    CGPoint		startPt;		// initial mouse location
    CGPoint		curPt;			// current mouse location
    int			trackingMode;		// tracking mode
    CSkObjectPtr	objPtr;			// current drawing object
};
typedef struct CanvasData   CanvasData;

//-----------------------------------------------------------------------------------
// To draw our document content (origin = bottomLeft) into the view, we need to
// - flip document coordinates such that origin = topLeft
// - offset by the document's position in the view and the scrollPosition
// - scale by the zoom factor.
static CGAffineTransform 
MakeDisplayTransform(float zoomFactor, float docHeight, CGPoint pageTopLeft, CGPoint scrollPosition)
{
    CGAffineTransform f = (docHeight == 0.0 ? CGAffineTransformIdentity : CGAffineTransformMake(1, 0, 0, -1, 0, docHeight));
    CGAffineTransform t = CGAffineTransformMakeTranslation(pageTopLeft.x - scrollPosition.x, pageTopLeft.y - scrollPosition.y);
    CGAffineTransform s = CGAffineTransformMakeScale(zoomFactor, zoomFactor);
    return CGAffineTransformConcat(CGAffineTransformConcat(f, t), s);
}

//-----------------------------------------------------------------------------------------------------------------------
OSStatus
OverlayViewHandler( EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon )
{
#pragma unused(inCaller)
    OSStatus	    err = eventNotHandledErr;
    DocStorage*	    docStP = (DocStorage*) inRefcon;
    const HIViewID  kDocViewID = { kDocumentViewSignature, 0 };
    HIViewRef	    docView;
    CanvasData*	    data;
    
    HIViewFindByID( docStP->theScrollView, kDocViewID, &docView );
    data = HIObjectDynamicCast( (HIObjectRef) docView, kCSkDocViewClassID );
    
    check( data != NULL );
    
    switch ( GetEventClass( inEvent ) )
    {
	case kEventClassControl:
	    switch ( GetEventKind( inEvent ) )
	    {
		case kEventControlDraw:
		{
		    const CGrgba	trStrokeColor   = { 0.5, 0.5, 0.5, 0.7 };   // gray with alpha = 0.7
		    const CGrgba	trFillColor     = { 0.9, 0.9, 0.9, 0.3 };   // ltgray with alpha = 0.3
		    
		    CGContextRef ctx;
		    verify_noerr( GetEventParameter( inEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof( ctx ), NULL, &ctx ) );
		    
		    // Set the fill and stroke colorspaces
		    CGColorSpaceRef genericColorSpace = GetGenericRGBColorSpace();
		    CGContextSetFillColorSpace(ctx, genericColorSpace); 
		    CGContextSetStrokeColorSpace(ctx, genericColorSpace); 
		    
		    // Clip to window content minus scroll bars
		    CGRect cgWRect;
		    HIViewGetFrame (docStP->theScrollView, &cgWRect);
		    cgWRect.size.width -= kScrollbarWidth;
		    cgWRect.size.height -= kScrollbarWidth;
		    CGContextClipToRect(ctx, cgWRect);
		    
		    CSkToolPaletteSetContextState(ctx, docStP->toolPalette);
		    CGContextSetStrokeColor(ctx, (CGFloat*)&trStrokeColor);
		    CGContextSetFillColor(ctx, (CGFloat*)&trFillColor);

		    CGAffineTransform m =  MakeDisplayTransform(data->zoomFactor, 
								docStP->pageRect.size.height,
								docStP->pageTopLeft, 
								data->scrollPosition );
		    CGContextConcatCTM(ctx, m);
		    
		    // And now (that the CTM is set up) also clip to the document bounds
		    CGContextClipToRect(ctx, docStP->pageRect);
    
		    CGContextClearRect(ctx, docStP->pageRect);

		    CGRect r = CGRectMake(data->startPt.x, data->startPt.y, data->curPt.x - data->startPt.x, data->curPt.y - data->startPt.y);
		    r = CGRectStandardize(r);	// convert it such that width and height are positive
		    
		    switch (data->trackingMode)
		    {
			case eDragSelection:
			    CGContextSetRGBFillColor(ctx, 0.2, 0.2, 0.2, 0.2);
			    CGContextFillRect(ctx, r);
			    break;

			case eMoveSelection:
			case eDuplicateSelection:
			    // redraw selected objects at offset cgEndPt - cgStartPt
			    RenderSelectedDrawObjs(ctx, &docStP->objList, data->curPt.x - data->startPt.x, data->curPt.y - data->startPt.y, 0.7);
			    break;
			    
			case eResizeViaGrabber:
			    RenderCSkObject(ctx, data->objPtr, true);
			    break;

			case eCreateObject:
			{
			    int shapeType = GetDrawObjShapeType(data->objPtr);
			    RenderCSkObject(ctx, data->objPtr, true);

			    if (shapeType == kFreePolygon)	// also add "loose end"
			    {
				if (shapeType == kFreePolygon)
				{
				    CGContextMoveToPoint(ctx, data->startPt.x, data->startPt.y);
				    CGContextAddLineToPoint(ctx, data->curPt.x, data->curPt.y);
				    CGContextStrokePath(ctx);
				}
			    }
			    break;
			}
		    }	// switch (trackingMode)
		    
		    CGContextSynchronize(ctx);

		    err = noErr;
		    break;
		}
	    }
	    break;
	
	default:
	    break;
    }
    
    return err;
}

//-----------------------------------------------------------------------------------
static void DrawTheDocumentView(CGContextRef ctx, CanvasData* data)
{
    const CGrgba docBackgroundColor = { 0.4, 0.6, 0.4, 1.0 };	    // used as float[4] in CGContextSetFillColor
    const CGrgba whiteColor	    = { 1.0, 1.0, 1.0, 1.0 };
    
    WindowRef	w = GetControlOwner(data->theView);
    DocStorage*	docStP = GetWindowDocStoragePtr(w);
    HIRect	viewBounds;

    HIViewGetBounds(data->theView, &viewBounds);
	
    // First, fill the whole background. The view system has set up the clip.
    CGContextSetFillColor(ctx, (CGFloat*)&docBackgroundColor);
    CGContextFillRect(ctx, viewBounds);
    
    CGSize docSize = docStP->pageRect.size;
    // Set up the display transform from document to view coordinates
    // (Should do this only when zoom, scrollPosition, docSize, pageTopLeft change!)
    CGAffineTransform m = MakeDisplayTransform( data->zoomFactor, 
						docSize.height, 
						docStP->pageTopLeft, 
						data->scrollPosition );
    docStP->displayCTM = m;
    CGContextConcatCTM(ctx, m);
    
    // Now (that the CTM is set up) also clip to the document bounds
    CGContextClipToRect(ctx, docStP->pageRect);

    // fill the page with white
    CGContextSetFillColor(ctx, (CGFloat*)&whiteColor);
    CGContextFillRect(ctx, CGRectMake(0, 0, docSize.width, docSize.height));
    
    // Now draw the page in regular document coordinates, indicating selected objects
    docStP->shouldDrawGrabbers = true;	// show selected objects
    DrawThePage(ctx, docStP);
    docStP->shouldDrawGrabbers = false;	// be default, don't show selected objects
}

//------------------------------------------------------------------------
// Make sure we always scroll in a position such that we don't display out-of-bounds content
static HIPoint SanityCheck(HIPoint where, CanvasData* data)
{
    HIRect bounds;
    HIViewGetBounds(data->theView, &bounds);

    bounds.size.height /= data->zoomFactor;
    bounds.size.width /= data->zoomFactor;

    if (where.y + bounds.size.height > data->canvasSize.height) 
	    where.y = data->canvasSize.height - bounds.size.height;
    if (where.y < 0) where.y = 0;

    if (where.x + bounds.size.width > data->canvasSize.width) 
	    where.x = data->canvasSize.width - bounds.size.width;
    if (where.x < 0) where.x = 0;
    
    return where;
}

//------------------------------------------------------------------------------------------------
static DocStorage* GetWindowStorageFromViewData(CanvasData* data)
{
    WindowRef   w = GetControlOwner(data->theView);
    DocStorage*	docStP = GetWindowDocStoragePtr(w);
    if (docStP == NULL)
	fprintf(stderr, "GetWindowDocStoragePtr returns NULL\n");
    return docStP;
}

// -----------------------------------------------------------------------------
static HIPoint QDGlobalToHIViewLocal( const Point inGlobalPoint, const HIViewRef inTheView)
{                               
    HIPoint viewPoint = CGPointMake(inGlobalPoint.h, inGlobalPoint.v);

    if (HIPointConvert != NULL)		// only available in 10.4 and later
    {
	HIPointConvert(&viewPoint, kHICoordSpace72DPIGlobal, NULL, kHICoordSpaceView, inTheView);
    }
    else
    {
	Rect bounds;
	GetWindowBounds(GetControlOwner(inTheView), kWindowStructureRgn, &bounds);
	viewPoint.x -= bounds.left;				// make it window-relative
	viewPoint.y -= bounds.top;
	HIViewConvertPoint(&viewPoint, NULL, inTheView);	// make it view-relative
    }
    return viewPoint;
}


//--------------------------------------------------------------------------------------
// We support "shiftKeyDown" to constrain shape creation to
// a) only horizontal, vertical or diagonal lines
// b) only square rectangles and ovals.

static void AdjustEndPoint(CGPoint* ioPt, CGPoint beginPt, int selectedShape, UInt32 modifiers)
{
    Boolean shiftKeyDown = ((modifiers & shiftKey) != 0);

    if ( shiftKeyDown && ((beginPt.x != ioPt->x) || (beginPt.y != ioPt->y)) )	// else nothing to do
    {
        float dx = ioPt->x - beginPt.x;
        float dy = ioPt->y - beginPt.y;
	float absDX = fabsf(dx);
	float absDY = fabsf(dy);
	float d = fmaxf(absDX, absDY);
	float sx = d * (dx / absDX);
	float sy = d * (dy / absDY);

        if (selectedShape == kLineShape)  // allow horizontal, vertical and diagonal only
        {
            if (absDX < absDY/4)
            {
                ioPt->x = beginPt.x;
            }
            else if (absDY < absDX/4)
            {
                ioPt->y = beginPt.y;
            }
            else    // diagonal
            {
		ioPt->x = beginPt.x + sx;
                ioPt->y = beginPt.y + sy;
            }
        }
        else    // make it square
        {
            ioPt->x = beginPt.x + sx;
            ioPt->y = beginPt.y + sy;
        }
    }
}	// AdjustEndPoint


//------------------------------------------------------------------------------------------------
static void DoHitTest(EventRef inEvent, CanvasData* data)
{
#pragma unused(data)
    HIPoint	    where;
    ControlPartCode part = kControlContentMetaPart;

    GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof(HIPoint), NULL, &where);
    SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &part);
}   // DoHitTest

//------------------------------------------------------------------------------------------------
static void DealWithNewMouseLocation(DocStorage* docStP, CanvasData* data,
				    CSkObjectPtr objPtr, 
				    CGPoint startPt, CGPoint curPt, 
				    int* hitCounter, 
				    int trackingMode, 
				    UInt32 modifiers)
{
    Boolean redrawOverlay = false;
    
    CGRect r = CGRectMake(startPt.x, startPt.y, curPt.x - startPt.x, curPt.y - startPt.y);
    r = CGRectStandardize(r);	// convert it such that width and height are positive
    
    data->trackingMode = trackingMode;
    data->objPtr = objPtr;
    data->startPt = startPt;
    data->curPt = curPt;
    
    switch (trackingMode)
    {
	case eDragSelection:
	    if ((modifiers & shiftKey) == 0)	// no shift key down - don't extend selection
		CSkObjListSetSelectState(&docStP->objList, false);
	    CSkObjListSelectWithinRect(&docStP->objList, r);
	    redrawOverlay = true;
	    break;

	case eMoveSelection:
	case eDuplicateSelection:
	    redrawOverlay = true;
	    break;
	    
	case eResizeViaGrabber:
	    CSkShapeResize(CSkObjectGetShape(objPtr), hitCounter, curPt);
	    redrawOverlay = true;
	    break;
	    
	case eCreateObject:
	    {
		int shapeType = GetDrawObjShapeType(objPtr);
		switch (shapeType)
		{
		    case kLineShape:
		    case kQuadBezier:
		    case kCubicBezier:
			CSkShapeSetPointAtIndex(CSkObjectGetShape(objPtr), curPt, *hitCounter);
			break;
			
		    case kRectShape:
		    case kOvalShape:
		    case kRRectShape:
			{
			    CGRect objBounds;
			    objBounds.origin = startPt;
			    objBounds.size = CGSizeMake(curPt.x - startPt.x, curPt.y - startPt.y);
			    objBounds = CGRectStandardize(objBounds);			// make size components positive
			    CSkShapeSetBounds(CSkObjectGetShape(objPtr), objBounds);
			}
			break;
			
		    case kFreePolygon:
			break;
		}
		
		redrawOverlay = true;
	    }
	    break;
	    
	default:
	    fprintf(stderr, "DealWithNewMouseLocation should not get called with trackingMode %d\n", trackingMode);
	    break;
    }	// switch (trackingMode)
    
    if ( redrawOverlay )
    {
	HIViewSetNeedsDisplay( docStP->overlayView, true );
	HIWindowFlush( docStP->overlayWindow );
    }
}   // DealWithNewMouseLocation

//------------------------------------------------------------------------------------------------
static void DealWithMouseReleased(DocStorage* docStP, CSkObjectPtr objPtr, 
				    CGPoint startPt, CGPoint curPt, 
				    int trackingMode)
{
    switch (trackingMode)
    {
	case eMoveSelection:
	    MoveSelectedDrawObjs(&docStP->objList, curPt.x - startPt.x, curPt.y - startPt.y);
	    break;
	    
	case eDuplicateSelection:
	    DuplicateSelectedDrawObjs(&docStP->objList, curPt.x - startPt.x, curPt.y - startPt.y);
	    break;
	    
	case eCreateObject:
	    {
		CSkObjectSetAttributes(objPtr, CSkToolPaletteGetAttributes(docStP->toolPalette));
		CSkObjListSetSelectState(&docStP->objList, false);
		SetDrawObjSelectState(objPtr, true);
		AddDrawObjToList(&docStP->objList, objPtr);	// transfer ownership to objList
		objPtr = NULL;	// don't release it!
	    }
	    break;
    }
}

//------------------------------------------------------------------------------------------------
static int DetermineTrackingMode(DrawObjListPtr objList, CSkObjectPtr hitObj, int hitGrabber, UInt32 modifiers, int shapeSelect)
{
    int trackingMode = eCreateObject;    // default when opening a new document and shapeSelect > 0
    
    if (hitObj == NULL)	// either createObject or dragSelection, depending on shapeSelect
    {
	// Deselect selected objects (if any) - except if the shift key is down
	if ((modifiers & shiftKey) == 0)	// don't extend selection - deselect everything
	{
	    CSkObjListSetSelectState(objList, false);
	}
	
	if (shapeSelect == 0)		    // "arrow"
	    trackingMode = eDragSelection;
	// else eCreateObject!
    }
    else // we did hit an object
    {
	// if it was selected already, start tracking; else select it and return eChangedSelectState
	if (IsDrawObjSelected(hitObj))
	{
	    if (hitGrabber > 0)
	    {
		trackingMode = eResizeViaGrabber;
	    }
	    else
	    {
		if ((modifiers & optionKey) != 0)
		    trackingMode = eDuplicateSelection;
		else
		    trackingMode = eMoveSelection;
	    }
	}
	else	// not yet selected
	{
	    if ((modifiers & shiftKey) == 0)	// don't extend selection - deselect everything
	    {
		CSkObjListSetSelectState(objList, false);
	    }
	    SetDrawObjSelectState(hitObj, true);
	    trackingMode = eChangedSelectState;
	}
    }
    return trackingMode;
}

//------------------------------------------------------------------------------
#if 0
static void ShowMouseTrackingResult(int trackingResult)
{
    switch (trackingResult)
    {
	case kMouseTrackingMouseDown	    : fprintf(stderr, "MouseDown\n"); break;
	case kMouseTrackingMouseUp	    : fprintf(stderr, "MouseUp\n"); break;
	case kMouseTrackingMouseExited	    : fprintf(stderr, "MouseExisted\n"); break;
	case kMouseTrackingMouseEntered	    : fprintf(stderr, "MouseEntered\n"); break;
	case kMouseTrackingMouseDragged	    : fprintf(stderr, "MouseDragged\n"); break;
	case kMouseTrackingKeyModifiersChanged : fprintf(stderr, "ModifiersChanged\n"); break;
	case kMouseTrackingUserCancelled    : fprintf(stderr, "Cancelled\n"); break;
	case kMouseTrackingTimedOut	    : fprintf(stderr, "TimedOut\n"); break;
	case kMouseTrackingMouseMoved	    : fprintf(stderr, "MouseMoved\n"); break;
    
    }
}
#endif

//------------------------------------------------------------------------------
static void DoMouseTracking(EventRef inEvent, CanvasData* data)
{
    WindowRef		w = GetControlOwner(data->theView);
    DocStorage*		docStP = GetWindowDocStoragePtr(w);
    CGPoint		where, startPt;
    ControlPartCode	part;
    UInt32		modifiers;
    int			clickCount = 0;
    
    ShowWindow(docStP->overlayWindow);
    
    CGAffineTransform m =  MakeDisplayTransform(data->zoomFactor, 
						docStP->pageRect.size.height,
						docStP->pageTopLeft, 
						data->scrollPosition );
    CGAffineTransform t = CGAffineTransformInvert(m);

    // Extract the mouse location (local coordinates!)
    GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof(HIPoint), NULL, &where);
    startPt = CGPointApplyAffineTransform(where, t);	// startPt is in document coordinates
    
    // Check if we hit one of our drawn objects, and if so, if we hit one of the "grabbers" for resizing.
    // Apply the inverse of the displayCTM to the local "where" point, so we can work in document coordinates.
    CGPoint	docPt = CGPointApplyAffineTransform(where, CGAffineTransformInvert(docStP->displayCTM));
    CSkObjectPtr hitObj = DrawObjListHitTesting(&docStP->objList, docStP->bmCtx, docStP->displayCTM, where, docPt, &data->hitGrabber);

    GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifiers);
    
    int  shapeSelect	= MapToolToShape(CSkToolPaletteGetTool(docStP->toolPalette));
    int  trackingMode	= DetermineTrackingMode(&docStP->objList, hitObj, data->hitGrabber, modifiers, shapeSelect);
    CSkObjectPtr objPtr = NULL;
    CSkShapePtr sh	= NULL;
    MouseTrackingResult lastTrackingResult = 0xFFFF;	// indicate that we are starting
    
//    fprintf(stderr, "Shape %d   ", shapeSelect);
//    ShowPoint("StartPt", startPt);
    
    data->mouseUpTracking = false;  // by default;
    
    switch (trackingMode)
    {
	case eCreateObject:
	    sh = CSkShapeCreate(shapeSelect);
	    objPtr = CreateCSkObj( CSkToolPaletteGetAttributes(docStP->toolPalette), sh);
	    SetDrawObjSelectState(objPtr, true);    // so we can see the grabber control lines during tracking

	    switch (shapeSelect)
	    {
		case kLineShape:
		case kQuadBezier:
		case kCubicBezier:  
		    CSkShapeSetPointAtIndex(sh, startPt, clickCount++);	// clickCount was 0
		    CSkShapeSetType(sh, clickCount);	// kLineShape for clickCount = 1, etc.
		    break;
		    
		case kFreePolygon:
		    CSkShapeAddPolygonPoint(sh, startPt);    // the "MoveTo" point
//		    ShowPoint("Polygon start", startPt);
		    break;
	    }
	    if ((shapeSelect == kQuadBezier) || (shapeSelect == kCubicBezier) || (shapeSelect == kFreePolygon))
		data->mouseUpTracking = true;

	    SetThemeCursor(kThemeCrossCursor);
	    break;
	    
	case eResizeViaGrabber:
	    objPtr = hitObj;
	    SetThemeCursor(kThemeCrossCursor);
	    break;

	case eMoveSelection:
	    SetThemeCursor(kThemeClosedHandCursor);
	    break;
	    
	case eDuplicateSelection:
	    SetThemeCursor(kThemeCopyArrowCursor);
	    break;
    }
    
    if (trackingMode != eChangedSelectState)	// else ignore mousetracking
    {
	Boolean	keepGoing = true;
	UInt32 lastClick = 0;
	
	while (keepGoing)
	{
	    Point	    qdPt;
	    CGPoint	    curPt;
	    MouseTrackingResult trackingResult;
	    UInt32	    modifiers;
	    
	    // Watch the mouse for change: qdPt comes back in global coordinates!
	    TrackMouseLocationWithOptions(NULL, 0, kEventDurationForever, &qdPt, &modifiers, &trackingResult );
	    where = QDGlobalToHIViewLocal(qdPt, data->theView);
	    curPt = CGPointApplyAffineTransform(where, t);
	    
	    if (data->mouseUpTracking)  // we only stop tracking after a double-click or when clickCount matches required pointCount
	    {
//		ShowMouseTrackingResult((int)trackingResult);
		if (trackingResult == kMouseTrackingMouseDown)
		{
		    clickCount += 1;
		     if (shapeSelect == kFreePolygon)	// stop on double-click
		    {
			UInt32 t = TickCount();
			keepGoing = (t - lastClick > GetDblTime());
			lastClick = t;
			CSkShapeAddPolygonPoint(sh, curPt);
		    }
		    else if (shapeSelect == kQuadBezier)    // stop if this is the last mouseDown for the type of curve
		    {
			keepGoing = (clickCount < 3);
			if (keepGoing)
			    CSkShapeSetType(sh, clickCount);	  // now track curve according to pointCount
		    }
		    else if (shapeSelect == kCubicBezier)
		    {
			keepGoing = (clickCount < 4);
			if (keepGoing)
			    CSkShapeSetType(sh, clickCount);	  // now track curve according to pointCount
		    }
		    
		    startPt = curPt;	// keep startPt for the "loose end"
		}
		lastTrackingResult = trackingResult;
	    }
	    else   // stop tracking at mouseUp
	    {
		keepGoing = (trackingResult != kMouseTrackingMouseUp);
	    }
	    
	    if (keepGoing)
	    {
		// If modifier keys are down, we may want to constrain the endPt (do it in document coordinates!)
		// - but only if we are not in "resizeViaGrabber" mode, to avoid too much extra code
		if (trackingMode != eResizeViaGrabber)
		    AdjustEndPoint(&curPt, startPt, shapeSelect, modifiers);
	    
		// fprintf(stderr, "track: (%3d, %3d)   xfd: (%3d, %3d)\n",(int)where.x, (int)where.y, (int)curPt.x, (int)curPt.y);
		if (trackingMode == eCreateObject)  // In this case, "hitGrabber" is unused. Use it for "clickCount", instead.
		    data->hitGrabber = clickCount;
		DealWithNewMouseLocation(docStP, data, objPtr, startPt, curPt, &data->hitGrabber, trackingMode, modifiers);
	    }
	    else
	    {
		// If we are drawing a line, but don't drag (i.e. we create a single point),
		// we need to fix up the second point in the line shape
		if ((shapeSelect == kLineShape) && CGPointEqualToPoint(startPt, curPt))
		{
		    CSkShapePtr sh = CSkObjectGetShape(objPtr);
		    CGPoint* pts = CSkShapeGetPoints(sh);
		    pts[1] = pts[0];
		}
		DealWithMouseReleased(docStP, objPtr, startPt, curPt, trackingMode);
		HideWindow(docStP->overlayWindow);
	    }
	    
	    HIViewSetNeedsDisplay(data->theView, true);
	    
	    part = kControlNoPart;
	    SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &part); 
	}
    }
    
    // Send back the part upon which the mouse was released
    part = kControlEntireControl;
    SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &part); 

    SetThemeCursor( kThemeArrowCursor );
}   // DoMouseTracking


//-----------------------------------------------------------------------------------
static OSStatus
DocumentViewHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    OSStatus	    err = eventNotHandledErr;
    UInt32	    eventKind = GetEventKind(inEvent);
    CanvasData*	    data = (CanvasData*)inUserData;
	    
    switch ( GetEventClass(inEvent) )
    {
	case kEventClassHIObject:   // the boilerplate HIObject business
	switch ( eventKind )
	{
	    case kEventHIObjectConstruct:
	    {
		data = (CanvasData*)malloc(sizeof(CanvasData));

		err = GetEventParameter( inEvent, kEventParamHIObjectInstance, typeHIObjectRef, NULL, sizeof(HIObjectRef*), NULL, &data->theView );
		require_noerr( err, ParameterMissing );

		SetEventParameter( inEvent, kEventParamHIObjectInstance, typeVoidPtr, sizeof(CanvasData), &data ); 
	    }
	    break;
	    
	    case kEventHIObjectInitialize:
	    err = CallNextEventHandler( inCallRef, inEvent );
	    if ( err == noErr )
	    {
		Rect bounds;
		// Extract the bounds from the initialization event that we set up
		err = GetEventParameter(inEvent, kDocumentBoundsParam, typeQDRectangle, NULL, sizeof(Rect), NULL, &bounds);
		require_noerr(err, ParameterMissing);
		SetControlBounds(data->theView, &bounds);
		// Also initialize our CanvasData
		data->scrollPosition = CGPointMake(bounds.left, bounds.top);
		data->canvasSize = CGSizeMake(bounds.right - bounds.left, bounds.bottom - bounds.top);
		data->zoomFactor = 1.0;
	    }
	    break;
	    
	    case kEventHIObjectDestruct:
		free(inUserData);
	    break;
	}
	break;	// kEventClassHIObject
	
	case kEventClassScrollable:	// obey the "Scrollable" protocol
	switch ( eventKind )
	{
	    case kEventScrollableGetInfo:
	    {
		// We're being asked to return in event parameters information about the scrolled view:
		// canvasSize, viewSize, scrollPosition, lineSize (i.e. scroll increment).
		// When the zoom factor changes, we keep the canvasSize constant, and instead change the 
		// viewSize by the reciprocal amount.
		
		HISize lineSize	= CGSizeMake(10, 10);	// or make it, say, 1/10 of the canvasSize?
		HIRect viewBounds;
		
		// Need to pick up zoomFactor from docStP, in case it has changed
		DocStorage* docStP = GetWindowStorageFromViewData(data);
		if (docStP == NULL)
		    data->zoomFactor = 1.0;
		else
		    data->zoomFactor = docStP->scale;
		
		HIViewGetBounds(HIViewGetSuperview(data->theView), &viewBounds);
		viewBounds.size.width /= data->zoomFactor;
		viewBounds.size.height /= data->zoomFactor;
		
		SetEventParameter(inEvent, kEventParamImageSize, typeHISize, sizeof(HISize), &data->canvasSize);
		SetEventParameter(inEvent, kEventParamViewSize, typeHISize, sizeof(HISize), &viewBounds.size);
		SetEventParameter(inEvent, kEventParamOrigin, typeHIPoint, sizeof(HIPoint), &data->scrollPosition);
		SetEventParameter(inEvent, kEventParamLineSize, typeHISize, sizeof(HISize), &lineSize);
		err = noErr;
	    }
	    break;
	    
	    case kEventScrollableScrollTo:
	    {
		// We're being asked to scroll: just do a sanity check and ask for a redraw if the location is different
		HIPoint where;
		GetEventParameter(inEvent, kEventParamOrigin, typeHIPoint, NULL, sizeof(HIPoint), NULL, &where);

		where = SanityCheck(where, data);

		if ((data->scrollPosition.y != where.y) || (data->scrollPosition.x != where.x))
		{
		    data->scrollPosition = where;
		    HIViewSetNeedsDisplay(data->theView, true);
		}
		err = noErr;
	    }
	    break;
	}
	break;	// kEventClassScrollable
	
	case kEventClassControl:    // draw, hit test and track
	switch ( eventKind )
	{
	    case kEventControlDraw:
	    {
		CGContextRef ctx;
		
		GetEventParameter(inEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof(CGContextRef), NULL, &ctx);
		CallNextEventHandler(inCallRef, inEvent);	    // Erase old content
		DrawTheDocumentView(ctx, data);
		err = noErr;
	    }
	    break;
	    
	    case kEventControlHitTest:
	    {
//		fprintf(stderr, "kEventControlHitTest\n");
		DoHitTest(inEvent, data);
		err = noErr;
	    }
	    break;

	    case kEventControlClick:
	    {
		UInt32 clickCount;
		GetEventParameter(inEvent, kEventParamClickCount, typeUInt32, NULL, sizeof(UInt32), NULL, &clickCount);
//		fprintf(stderr, "kEventControlClick; clickCount = %d\n", (int)clickCount);
		if (clickCount >= 2)
		    data->mouseUpTracking = false;
	    }
	    break;
	    
	    case kEventControlTrack:
	    {
//		fprintf(stderr, "kEventControlTrack\n");
		DoMouseTracking(inEvent, data);
		err = noErr;
	    }
	    break;

/* -- doesn't look like we need this --			
	    case kEventControlGetPartRegion:
	    {
		ControlPartCode	part;
		RgnHandle rgn;
		GetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, NULL, sizeof(ControlPartCode), NULL, &part );
		GetEventParameter(inEvent, kEventParamControlRegion, typeQDRgnHandle, NULL, sizeof(RgnHandle), NULL, &rgn );
//				err = GetRegion( part, rgn );
		Rect r;
		r.top = data->scrollPosition.y;
		r.left = data->scrollPosition.x;
		r.bottom = data->scrollPosition.y + data->canvasSize.height;
		r.right = data->scrollPosition.x + data->canvasSize.width;
		RectRgn(rgn, &r);
		err = noErr;
	    }
	    break;
*/
	}
	break;	// kEventClassControl
    }

ParameterMissing:
    return err;
}

//-----------------------------------------------------------------------------------
static OSStatus MyDocumentViewRegister(CFStringRef myClassID)
{
    OSStatus                err = noErr;
    static HIObjectClassRef sMyViewClassRef = NULL;
	
    if ( sMyViewClassRef == NULL )
    {
        EventTypeSpec eventList[] = {
            { kEventClassHIObject, kEventHIObjectConstruct },
            { kEventClassHIObject, kEventHIObjectInitialize },
            { kEventClassHIObject, kEventHIObjectDestruct },

            { kEventClassControl, kEventControlDraw },
            { kEventClassControl, kEventControlHitTest },
	    { kEventClassControl, kEventControlClick },
	    { kEventClassControl, kEventControlTrack },
//          { kEventClassControl, kEventControlGetPartRegion },

	    { kEventClassCommand, kEventCommandProcess },
	    
	    { kEventClassScrollable, kEventScrollableGetInfo },
	    { kEventClassScrollable, kEventScrollableScrollTo }
    };
		
    err = HIObjectRegisterSubclass( myClassID,
				    kHIViewClassID,			// base class ID
				    0,					// option bits
				    DocumentViewHandler,		// construct proc
				    GetEventTypeCount( eventList ),
				    eventList,
				    NULL,				// construct data,
				    &sMyViewClassRef );
    }
    return err;
}

//-----------------------------------------------------------------------------------
OSStatus CSkDocumentViewCreate(HIViewRef parentView, const Rect* inBounds, const HIViewID* inViewID)
{
    OSStatus	err;
    EventRef	event;
    HIViewRef	theView;
	
    // Register this class
    err = MyDocumentViewRegister(kCSkDocViewClassID); 
    require_noerr( err, CantRegister );
	
    // Make an initialization event
    err = CreateEvent( NULL, kEventClassHIObject, kEventHIObjectInitialize, GetCurrentEventTime(), 0, &event ); 
    require_noerr( err, CantCreateEvent );
        
    // If bounds were specified, push them into the initialization event
    // so that they can be used in the initialization handler.
    if ( inBounds != NULL )
    {
        err = SetEventParameter(event, kDocumentBoundsParam, typeQDRectangle, sizeof(Rect), inBounds);
        require_noerr( err, CantSetParameter );
    }
    err = HIObjectCreate(kCSkDocViewClassID, event, (HIObjectRef*)&theView);
    require_noerr(err, CantCreate);
	
    if (parentView != NULL) 
    {
        err = HIViewAddSubview(parentView, theView);
    }
	SetControlID(theView, inViewID);
	HIViewSetVisible(theView, true);
	
CantCreate:
CantSetParameter:
CantCreateEvent:
    ReleaseEvent( event );
	
CantRegister:
    return err;
}
