/*
    File:       CSkShapes.c
        
    Contains:	Definition of (opaque) CSkShape structure, and various related routines.
                The idea is to facilitate future expansion by additional shapes, without
                going object-oriented with polymorphism (which it should still suggest and
                facilitate as well).
                
    Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                ("Apple") in consideration of your agreement to the following terms, and your
                use, installation, modification or redistribution of this Apple software
                constitutes acceptance of these terms.  If you do not agree with these terms,
                please do not use, install, modify or redistribute this Apple software.

                In consideration of your agreement to abide by the following terms, and subject
                to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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

    Copyright © 2004-2005 Apple Computer, Inc., All Rights Reserved
*/


#include "CSkShapes.h"
#include "CSkConstants.h"
#include "CSkUtils.h"

// The information about a CSkObject's geometric shape has been factored out into this separate file,
// for good coding practice, and to make room for future extensions.
// As it stands, CSkShapes are fixed-size allocations; but this might change in the future.

struct CSkRRect 
{
    CGRect  bounds;
    float   rX;
    float   rY;
};
typedef struct CSkRRect CSkRRect;

struct CSkShape 
{
    int		shapeType;
    union   {
	CGPoint			points[4];	// for lines, quadratic or cubic splines
        CGRect			bounds;		// for rects, ovals (and RRects)
        CSkRRect		rrect;
	CGMutablePathRef	path;		// for freePolygon and general paths
    } u;
};

#if 0
//------------------------------------------------------------------------------
static void CSkShapeSetRect(CSkShape* sh, CGRect rect)
{
    sh->shapeType = kRectShape;
    sh->u.bounds = rect;
}
#endif

//------------------------------------------------------------------------------
static void CSkShapeSetRRect(CSkShape* sh, CGRect rect, float rX, float rY)
{
    sh->shapeType = kRRectShape;
    sh->u.rrect.bounds = rect;
    sh->u.rrect.rX = rX;
    sh->u.rrect.rY = rY;
}

//------------------------------------------------------------------------------
// Allocate and initialize
CSkShapePtr CSkShapeCreate(int shapeType)
{
    const float cR = 16.0;      // default rounding radius for RRects
    CSkShapePtr sh = (CSkShapePtr)calloc(sizeof(CSkShape), 1);
    sh->shapeType = shapeType;
    if (shapeType == kRRectShape)
	CSkShapeSetRRect(sh, CGRectZero, cR, cR);
    // All other fields are 0
    return sh;
}

//--------------------------------------------------------
static bool CSkShapeUsesPath(const CSkShape* sh)
{
    int shapeType = CSkShapeGetType(sh);
    return (shapeType == kFreePolygon);    // for now, that's the only case where the sh->u.path is being used
}

//-------------------------------------------------------- Deallocate
void CSkShapeRelease(CSkShape* sh)
{
    if (CSkShapeUsesPath(sh))
	CGPathRelease(sh->u.path);
    free(sh);
}


//--------------------------------------------------------
ByteCount CSkShapeSize(void)
{
    return sizeof(CSkShape);
}

//-------------------------------------------------------- The expected accessors

CGPoint* CSkShapeGetPoints(CSkShapePtr sh)
{
    return sh->u.points;
}

//--------------------------------------------------------------------
void CSkShapeSetPointAtIndex(CSkShape* sh, CGPoint pt, int index)
{
    if ((index >= 0) && (index < 4))
    {
	sh->u.points[index] = pt;
//	fprintf(stderr, "points[%d] = (%g, %g)\n", index, pt.x, pt.y);
    }
    else
	fprintf(stderr, "CSkShapeSetPointAtIndex: index %d out of range\n", index);
}

//--------------------------------------------------------------------
int CSkShapeGetType(const CSkShape* sh)
{
    return sh->shapeType;
}

//--------------------------------------------------------------------
void CSkShapeSetType(CSkShapePtr sh, int shapeType)
{
    sh->shapeType = shapeType;
}

//------------------------------------------------------------------------------
static CGRect MakeBoundsFromPoints(CGPoint* pts, int pointCount)
{
    float xmin = 1e10;
    float xmax = -1e10;
    float ymin = 1e10;
    float ymax = -1e10;
    int i = 0;
    while (i < pointCount)
    {
	CGPoint pt = pts[i];
	if (pt.x < xmin)	xmin = pt.x;
	if (pt.x > xmax)	xmax = pt.x;
	if (pt.y < ymin)	ymin = pt.y;
	if (pt.y > ymax)	ymax = pt.y;
	i += 1;
    }
    return CGRectMake(xmin, ymin, xmax - xmin, ymax - ymin);
}

//------------------------------------------------------------------------------
CGRect CSkShapeGetBounds(CSkShape* sh)
{
    CGRect  bounds;
    
    switch (sh->shapeType)
    {
        case kLineShape:    bounds = MakeBoundsFromPoints(sh->u.points, 2);	break;
	case kQuadBezier:   bounds = MakeBoundsFromPoints(sh->u.points, 3);	break;
	case kCubicBezier:  bounds = MakeBoundsFromPoints(sh->u.points, 4);	break;
        case kRectShape:
        case kOvalShape:    bounds = sh->u.bounds;				break;
        case kRRectShape:   bounds = sh->u.rrect.bounds;			break;
	default:	    bounds = CGPathGetBoundingBox(sh->u.path);		break;
    }
    return bounds;
}

//------------------------------------------------------------------------------
CGPoint CSkShapeGetRRectRadii(const CSkShape* sh)
{
    return CGPointMake(sh->u.rrect.rX, sh->u.rrect.rY);
}

//------------------------------------------------------------------------------
CGMutablePathRef CSkShapeGetPath(const CSkShape* sh)
{
    CGMutablePathRef path = NULL;
    
    if (CSkShapeUsesPath(sh))
	path = sh->u.path;
	    
    return path;
}

//------------------------------------------------------------------------------
void CSkShapeSetBounds(CSkShape* sh, CGRect rect)
{
    if ((sh->shapeType >= kRectShape) && (sh->shapeType <= kRRectShape))
    {
        if (sh->shapeType != kRRectShape)
            sh->u.bounds = rect;
        else
	    sh->u.rrect.bounds = rect;
    }
    else
    {
//	fprintf(stderr, "CSkShapeSetBounds: shapeType %d not allowed\n", sh->shapeType);
    }
}

//------------------------------------------------------------------------------
static void CSkShapeSetPath(CSkShape* sh, CGMutablePathRef path)
{
    if (sh->u.path != NULL)
	CGPathRelease(sh->u.path);
    sh->u.path = (CGMutablePathRef) CGPathRetain(path);
}

//--------------------------------------------------------------
// (Cannot think of any non-redundant comment ...)
void CSkShapeOffset(CSkShape* sh, float offsetX, float offsetY)
{
    switch (sh->shapeType)
    {
        case kLineShape:
        case kQuadBezier:
        case kCubicBezier:
        {
            int i;
	    for (i = 0; i < 4; ++i)
	    {
		CGPoint p = sh->u.points[i];
		p.x += offsetX;
		p.y += offsetY;
		sh->u.points[i] = p;
	    }
        }
        break;
        
        case kRectShape:
        case kOvalShape:
        case kRRectShape:
        {
            CGRect bounds = CSkShapeGetBounds(sh);
            bounds = CGRectOffset(bounds, offsetX, offsetY);
            CSkShapeSetBounds(sh, bounds);
        }
        break;
		
	case kFreePolygon:
	{	
	    CGMutablePathRef newPath = CopyPathWithOffset(sh->u.path, offsetX, offsetY);
	    CGPathRelease(sh->u.path);
	    sh->u.path = newPath;
	}
	break;
    }
}


//------------------------------------------------------------------------------
// Here come the "grabbers":
// little squares to indicate selection and to grab for resizing.
// Lines are a special case (only start and end-point); all other shapes
// currently get 8 grabbers along the bounding box (four at corners,
// four at mid-sides).
// Once we add arcs or even general paths to the repertory, this grabbing
// business will get much more complicated!

enum {
    grGrabNone      = 0,
    grBottomLeft    = 1,
    grBottomMiddle  = 2,
    grBottomRight   = 3,
    grLeftMiddle    = 4,
    grRightMiddle   = 5,
    grTopLeft       = 6,
    grTopMiddle     = 7,
    grTopRight      = 8,
};

// Iterates through the obj's little grabber rectangles. Pass in 0 for *ioGrabber at 
// the beginning; it gets incremented on return. Keep going until return value is false.
// Called from "FindGrabberHit", below, and also from RenderCSkObject when they need
// to be drawn.
Boolean NextGrabberRect(CSkShape* sh, int* ioGrabber, CGRect* grabRect)
{
    const float kGrabberSize    = 8.0;
    const float kHalfGrabSize   = 0.5 * kGrabberSize;
    CGRect  grabR = CGRectMake( -kHalfGrabSize, -kHalfGrabSize, kGrabberSize, kGrabberSize);
    int	  grNum = *ioGrabber;
    
    if ((sh->shapeType == kLineShape) || (sh->shapeType == kQuadBezier) || (sh->shapeType == kCubicBezier))
    {
        if (grNum > sh->shapeType)
        {
            *ioGrabber = 0;
            return false;
        }
        *grabRect = CGRectOffset( grabR, sh->u.points[grNum].x, sh->u.points[grNum].y );
    }
    else if (sh->shapeType != kFreePolygon)
    {
        const int   xD[] = { 0, 1, 2, 0, 2, 0, 1, 2 };
        const int   yD[] = { 0, 0, 0, 1, 1, 2, 2, 2 };
        CGRect  bounds;
        float   halfWidth, halfHeight;
        
        if (grNum > 7)
        {
            *ioGrabber = 0;
            return false;
        }
        bounds      = CSkShapeGetBounds(sh);
        halfWidth   = 0.5 * CGRectGetWidth(bounds);
        halfHeight  = 0.5 * CGRectGetHeight(bounds);
        *grabRect   = CGRectOffset(grabR, bounds.origin.x + xD[grNum] * halfWidth,
                                          bounds.origin.y + yD[grNum] * halfHeight );
    }
    else // control points of path
    {
	// For now, only polygons ...
	static CGPoint* sPtArray = NULL;
	static int		sNumPoints = 0;
	
	if (grNum == 0)	// first request: allocate array of positions
	{
	    // Run through path to extract control points
	    sPtArray = ExtractControlPoints(CSkShapeGetPath(sh), &sNumPoints);
	}
	
	if (grNum > sNumPoints - 1)	// this was the last one - time to deallocate
	{
	    free(sPtArray);
	    sPtArray = NULL;
	    sNumPoints = 0;
	    *ioGrabber = 0;
	    return false;
	}
	else 
	{
	    *grabRect = CGRectOffset( grabR, sPtArray[grNum].x, sPtArray[grNum].y );
	}
    }
	
    *ioGrabber = grNum + 1;
    return true;
}

//--------------------------------------------------------------------------------------
int FindGrabberHit(CSkShape* shape, CGPoint pt)
{
    CGRect  grabRect;
    int		grabNum = 0;
    
    while (NextGrabberRect(shape, &grabNum, &grabRect))
    {
        if (CGRectContainsPoint(grabRect, pt))
            break;
    }
    return grabNum;
}

//--------------------------------------------------------------------------------------
// If the movement of the "dragger" causes size.width or size.height to become negative,
// the grabber changes its number! Don't know if there is a less messy way to deal with it.

void CSkShapeResize(CSkShape* sh, int* grabberNum, CGPoint newPt)
{	
    int shapeType = sh->shapeType;
    if ((shapeType == kLineShape) || (shapeType == kQuadBezier) || (shapeType == kCubicBezier))
    {
	if (*grabberNum <= shapeType + 1)   // uses special values of shapeType enums!
	sh->u.points[*grabberNum - 1] = newPt;
    }
    else if (shapeType == kRectShape || shapeType == kOvalShape || shapeType == kRRectShape)
    {
        CGRect  bounds = CSkShapeGetBounds(sh);
        float   dx, dy;
		
        switch (*grabberNum)
        {
            case grBottomLeft:
                dx = newPt.x - bounds.origin.x;
                dy = newPt.y - bounds.origin.y;
                bounds.origin.x += dx;
                bounds.origin.y += dy;
                bounds.size.width -= dx;
                bounds.size.height -= dy;
				if (bounds.size.width < 0)
					*grabberNum = grBottomRight;
				if (bounds.size.height < 0)
					*grabberNum = grTopLeft;
                break;
                
            case grBottomMiddle:
                dy = newPt.y - bounds.origin.y;
                bounds.origin.y += dy;
                bounds.size.height -= dy;
				if (bounds.size.height < 0)
					*grabberNum = grTopMiddle;
                break;
				
            case grBottomRight:
                dx = newPt.x - (bounds.origin.x + bounds.size.width);
                dy = newPt.y - bounds.origin.y;
                bounds.size.width += dx;
                bounds.origin.y += dy;
                bounds.size.height -= dy;
		if (bounds.size.width < 0)
			*grabberNum = grBottomLeft;
		if (bounds.size.height < 0)
			*grabberNum = grTopRight;
	    break;
				
            case grLeftMiddle:
                dx = newPt.x - bounds.origin.x;
                bounds.origin.x += dx;
                bounds.size.width -= dx;
		if (bounds.size.width < 0)
			*grabberNum = grRightMiddle;
	    break;
				
            case grRightMiddle:
                dx = newPt.x - (bounds.origin.x + bounds.size.width);
                bounds.size.width += dx;
		if (bounds.size.width < 0)
			*grabberNum = grLeftMiddle;
	    break;
				
            case grTopLeft:
                dx = newPt.x - bounds.origin.x;
                dy = newPt.y - (bounds.origin.y + bounds.size.height);
                bounds.origin.x += dx;
                bounds.size.width -= dx;
                bounds.size.height += dy;
		if (bounds.size.width < 0)
			*grabberNum = grTopRight;
		if (bounds.size.height < 0)
			*grabberNum = grBottomLeft;
	    break;
				
            case grTopMiddle:
                dy = newPt.y - (bounds.origin.y + bounds.size.height);
                bounds.size.height += dy;
		if (bounds.size.height < 0)
			*grabberNum = grBottomMiddle;
                break;
				
            case grTopRight:
                dx = newPt.x - (bounds.origin.x + bounds.size.width);
                dy = newPt.y - (bounds.origin.y + bounds.size.height);
                bounds.size.width += dx;
                bounds.size.height += dy;
		if (bounds.size.width < 0)
		    *grabberNum = grTopLeft;
		if (bounds.size.height < 0)
		    *grabberNum = grBottomRight;
		break;
        }
        CSkShapeSetBounds(sh, CGRectStandardize(bounds));
    }
    else	// need to rebuild the path with the <*grabberNum> control point replaced
    {
	CGMutablePathRef newPath = CreateResizedPath(sh->u.path, *grabberNum, newPt);
	CGPathRelease(sh->u.path);
	sh->u.path = newPath;
    }
	
}	// CSkShapeResize


//--------------------------------------------------------------------
void CSkShapeAddPolygonPoint(CSkShape* sh, CGPoint pt)
{
//  fprintf(stderr, "AddPt (%g, %g)\n", pt.x, pt.y);
    if (sh->shapeType != kFreePolygon)
    {
	fprintf(stderr, "CSkShapeAddPolygonPoint: wrong shapeType\n");
	return;
    }
    
    if (sh->u.path == NULL)
    {
	sh->u.path = CGPathCreateMutable();
    }

    if (CGPathIsEmpty(sh->u.path))
    {
	CGPathMoveToPoint(sh->u.path, NULL, pt.x, pt.y);
    }
    else
    {
	CGPathAddLineToPoint(sh->u.path, NULL, pt.x, pt.y);
    }
}


//------------------------------------------------------------------------------
void AddCSkShapeToDict(CSkShape* sh, CFMutableDictionaryRef objDict)
{
    CFNumberRef shapeType = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &sh->shapeType);
    CFDictionaryAddValue(objDict, kKeyShapeType, shapeType);
    CFRelease(shapeType);
    
    switch (sh->shapeType)
    {
	case kRectShape:
	case kOvalShape:
	    AddFloatToDict(objDict, kX1, sh->u.bounds.origin.x);
	    AddFloatToDict(objDict, kY1, sh->u.bounds.origin.y);
	    AddFloatToDict(objDict, kX2, sh->u.bounds.size.width);
	    AddFloatToDict(objDict, kY2, sh->u.bounds.size.height);
	    break;
	    
	case kRRectShape:
	    AddFloatToDict(objDict, kX1, sh->u.rrect.bounds.origin.x);
	    AddFloatToDict(objDict, kY1, sh->u.rrect.bounds.origin.y);
	    AddFloatToDict(objDict, kX2, sh->u.rrect.bounds.size.width);
	    AddFloatToDict(objDict, kY2, sh->u.rrect.bounds.size.height);
	    AddFloatToDict(objDict, krX, sh->u.rrect.rX);
	    AddFloatToDict(objDict, krY, sh->u.rrect.rY);
	    break;
	    
	case kCubicBezier:
	    AddFloatToDict(objDict, kX3, sh->u.points[3].x);
	    AddFloatToDict(objDict, kY3, sh->u.points[3].y);
	    // fall through
	    
	case kQuadBezier:
	    AddFloatToDict(objDict, kX2, sh->u.points[2].x);
	    AddFloatToDict(objDict, kY2, sh->u.points[2].y);
	    // fall through
	    
	case kLineShape:
	    AddFloatToDict(objDict, kX1, sh->u.points[1].x);
	    AddFloatToDict(objDict, kY1, sh->u.points[1].y);
	    AddFloatToDict(objDict, kX0, sh->u.points[0].x);
	    AddFloatToDict(objDict, kY0, sh->u.points[0].y);
	    break;
	    	    
	case kFreePolygon:
	    CFDictionaryAddValue(objDict, kPath, sh->u.path);
	    break;
    }
}

//------------------------------------------------------------------------------
CSkShape* CreateCSkShapeFromDict(CFDictionaryRef objDict)
{
    int shapeType = GetIntegerFromDict(objDict, kKeyShapeType);
    CSkShape* sh = CSkShapeCreate(shapeType);
    switch (shapeType)
    {
	case kRectShape:
	case kOvalShape:
	    sh->u.bounds.origin.x = GetFloatFromDict(objDict, kX1);
	    sh->u.bounds.origin.y = GetFloatFromDict(objDict, kY1);
	    sh->u.bounds.size.width = GetFloatFromDict(objDict, kX2);
	    sh->u.bounds.size.height = GetFloatFromDict(objDict, kY2);
	break;
	
	case kRRectShape:
	    sh->u.rrect.bounds.origin.x = GetFloatFromDict(objDict, kX1);
	    sh->u.rrect.bounds.origin.y = GetFloatFromDict(objDict, kY1);
	    sh->u.rrect.bounds.size.width = GetFloatFromDict(objDict, kX2);
	    sh->u.rrect.bounds.size.height = GetFloatFromDict(objDict, kY2);
	    sh->u.rrect.rX = GetFloatFromDict(objDict, krX);
	    sh->u.rrect.rY = GetFloatFromDict(objDict, krY);
	break;
		
	case kCubicBezier:
	    sh->u.points[3].x = GetFloatFromDict(objDict, kX3);
	    sh->u.points[3].y = GetFloatFromDict(objDict, kY3);
	    // fall through

	case kQuadBezier:
	    sh->u.points[2].x = GetFloatFromDict(objDict, kX2);
	    sh->u.points[2].y = GetFloatFromDict(objDict, kY2);
	    // fall through
	    
	case kLineShape:
	    sh->u.points[1].x = GetFloatFromDict(objDict, kX1);
	    sh->u.points[1].y = GetFloatFromDict(objDict, kY1);
	    sh->u.points[0].x = GetFloatFromDict(objDict, kX0);
	    sh->u.points[0].y = GetFloatFromDict(objDict, kY0);
	break;
		
	case kFreePolygon:
	{
	    CGMutablePathRef path = (CGMutablePathRef)CFDictionaryGetValue(objDict, kPath);
	    CSkShapeSetPath(sh, path);
	}
	break;
    }
    return sh;
}
