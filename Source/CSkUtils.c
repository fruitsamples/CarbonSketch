/*
    File:       CSkUtils.h
    
    Contains:	Some utility routines for CarbonSketch

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

#include "CSkUtils.h"
#include "CSkConstants.h"


//--------------------------------------------------
void SendWindowCloseEvent( WindowRef window )
{
    EventRef event;
    
    CreateEvent( NULL, kEventClassWindow, kEventWindowClose, GetCurrentEventTime(), kEventAttributeUserEvent, &event );
    SetEventParameter( event, kEventParamDirectObject, typeWindowRef, sizeof(window), &window );
    SendEventToWindow( event, window );
    ReleaseEvent( event );
}

//--------------------------------------------------
void SendWindowCommandEvent( WindowRef window, HICommand* command )
{
    EventRef event;
    
    CreateEvent( NULL, kEventClassCommand, kEventCommandProcess, GetCurrentEventTime(), kEventAttributeUserEvent, &event );
    SetEventParameter( event, kEventParamDirectObject, typeHICommand, sizeof(HICommand), command );
    SendEventToWindow( event, window );
    ReleaseEvent( event );
}

//--------------------------------------------------
void SendControlEventHit(ControlRef control)
{
    EventRef event;
    
    CreateEvent( NULL, kEventClassControl, kEventControlHit, GetCurrentEventTime(), kEventAttributeUserEvent, &event );
    SetEventParameter( event, kEventParamDirectObject, typeControlRef, sizeof(control), control );
    SendEventToEventTarget( event, GetControlEventTarget(control) );
    ReleaseEvent( event );
}

//---------------------------------------------------------------------------------
void ConvertRGBColorToCGrgba(const RGBColor* inRGB, CGFloat alpha, CGrgba* outCGrgba)
{
    outCGrgba->r = (CGFloat)inRGB->red   / 65535.0;
    outCGrgba->g = (CGFloat)inRGB->green / 65535.0;
    outCGrgba->b = (CGFloat)inRGB->blue  / 65535.0;
    outCGrgba->a = alpha;
}

static void ConvertCGrgbaToRGB(const CGrgba* inCGrgba, RGBColor* outRGB)
{
    outRGB->red     = 65535 * inCGrgba->r;
    outRGB->green   = 65535 * inCGrgba->g;
    outRGB->blue    = 65535 * inCGrgba->b;
}

#define USECALCOLOR 1


//----------------------------------------------------------------------------------
//  This function locates, opens, and returns the profile reference for the calibrated 
//  Generic RGB color space. It is up to the caller to call CMCloseProfile when done
//  with the profile reference this function returns.

static CMProfileRef OpenGenericProfile(void)
{
#define	kGenericRGBProfilePathStr       "/System/Library/ColorSync/Profiles/Generic RGB Profile.icc"
    static CMProfileRef cachedRGBProfileRef = NULL;
    
    // we only create the profile reference once
    if (cachedRGBProfileRef == NULL)
    {
	OSStatus	    err;
	CMProfileLocation   loc;

	loc.locType = cmPathBasedProfile;
	strcpy(loc.u.pathLoc.path, kGenericRGBProfilePathStr);

	err = CMOpenProfile(&cachedRGBProfileRef, &loc);
	
	if (err != noErr)
	{
	    cachedRGBProfileRef = NULL;
	    // log a message to the console
	    fprintf(stderr, "couldn't open generic profile due to error %d\n", (int)err);
	}
    }

    if (cachedRGBProfileRef)
    {
	// clone the profile reference so that the caller has their own reference, not our cached one
	CMCloneProfileRef(cachedRGBProfileRef);   
    }

    return cachedRGBProfileRef;
}

//-------------------------------------------
OSStatus PickSomeColor(CGrgba* theColor)
{
    RGBColor            rgb;
    NColorPickerInfo    info;
    OSStatus            err;
#if USECALCOLOR
    static CMProfileRef genericProfileRef = NULL;
#endif    
    
    memset(&info, 0, sizeof(NColorPickerInfo));
    ConvertCGrgbaToRGB(theColor, &rgb);

    info.placeWhere         = kCenterOnMainScreen;
    info.flags              = kColorPickerDialogIsMoveable | kColorPickerDialogIsModal;
    info.theColor.color.rgb.red     = rgb.red;
    info.theColor.color.rgb.green   = rgb.green;
    info.theColor.color.rgb.blue    = rgb.blue;

#if USECALCOLOR
    if (genericProfileRef == NULL)
    {
	//  We only call OpenGenericProfile once. It returns a reference that we "own".
	//  Since we hang on to that reference for the life of this application we
	//  don't close it after we are done in this function; we keep it in a static 
	//  variable so we don't have to get a reference at any later time.
	genericProfileRef = OpenGenericProfile();
    }
	
    info.dstProfile = genericProfileRef;
#endif

// restore call to NPickColor later once 64-bit ColorPicker is available
#if !__LP64__
    err = NPickColor(&info);
#else
    err = paramErr;
#endif
    require_noerr(err, NPickColorFAILED);
    
    if ((err == noErr) && info.newColorChosen)
    {
        rgb.red     = info.theColor.color.rgb.red;
        rgb.green   = info.theColor.color.rgb.green;
        rgb.blue    = info.theColor.color.rgb.blue;
        ConvertRGBColorToCGrgba(&rgb, theColor->a, theColor);
    }
            
NPickColorFAILED:
    return err;
}

//---------------------------------------------------------------------------------------------
// Map kLineTool, kRectTool, kOvalTool, kRRectTool, kPolygonTool, kQuadTool, kCubicTool
//  to kLineShape, kRectShape, kOvalShape, kRRectShape, kFreePolygonShape, kQuadBezier, kCubicBezier
int MapToolToShape(int toolID)
{
    const int map[] = { 0, 0, 1, 4, 5, 6, 7, 2, 3 };
    if ((toolID >= 0) && (toolID < 9))
	return map[toolID];
    fprintf(stderr, "MapToolToShape: toolID out of range\n");
    return 0;
}


//-----------------------------------------------------------------------------
//  Return the generic RGB color space. This is a 'get' function and the caller 
//  should not release the returned value unless the caller retains it first. 
//  Usually callers of this routine will immediately use the returned colorspace 
//  with CoreGraphics so they typically do not need to retain it themselves.
    
//  This function creates the generic RGB color space once and hangs onto it so it can
//  return it whenever this function is called.

CGColorSpaceRef GetGenericRGBColorSpace(void)
{
    static CGColorSpaceRef genericRGBColorSpace = NULL;

    if (genericRGBColorSpace == NULL)
    {
	genericRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    }
    return genericRGBColorSpace;
}


//-------------------------------------------------------------
// The API PasteboardCreate is new on 10.3 systems, but this application is targeted
// at Mac OS X 10.2 and later systems.  By defining in the targets build settings
// MACOSX_DEPLOYMENT_TARGET = 10.2 we are telling the build system to weak link
// API's that aren't available on the target system.  We can then
// check PasteboardCreate availibility before calling it by checking
// if PasteboardCreate != NULL as shown in GetPasteboard().
// See AvailabilityMacros.h for more information about weaklinking.
//
// If a paste board has alread been created return it. Otherwise check if
// the API PasteboardCreate can be weaklinked and if so try to create
// one to return.  Returns NULL if the pasteboard can't be created.
PasteboardRef GetPasteboard(void)
{
    static PasteboardRef sPasteboard = NULL;
    
    // If the pasteboard hasn't yet been created, and the API PasteboardCreate
    // is availible then attempt to create it.
    if (sPasteboard == NULL && PasteboardCreate != NULL)
    {
	PasteboardCreate( kPasteboardClipboard, &sPasteboard );
	if (sPasteboard == NULL)
	{
	    fprintf(stderr, "PasteboardCreate failed - I wonder why?!\n");
	}
    }
    return sPasteboard;
}



//------------------------------------------------------------------------------
void AddIntegerToDict(CFMutableDictionaryRef objDict, CFStringRef key, int value)
{
    CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFDictionaryAddValue(objDict, key, v);
    CFRelease(v);
}

//------------------------------------------------------------------------------
int GetIntegerFromDict(CFDictionaryRef theDict, CFStringRef key)
{
    CFNumberRef number = CFDictionaryGetValue(theDict, key);
    int	    n;
    if (CFNumberGetValue(number, kCFNumberIntType, &n) == false)
    {
	n = -1;
    }
    return n;
}

//------------------------------------------------------------------------------
void AddFloatToDict(CFMutableDictionaryRef objDict, CFStringRef key, float value)
{
    CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value);
    CFDictionaryAddValue(objDict, key, v);
    CFRelease(v);
}

//------------------------------------------------------------------------------
float GetFloatFromDict(CFDictionaryRef theDict, CFStringRef key)
{
    CFNumberRef number = CFDictionaryGetValue(theDict, key);
    float   n;
    if (CFNumberGetValue(number, kCFNumberFloatType, &n) == false)
    {
	n = -1;
    }
    return n;
}

//------------------------------------------------------------------------------
void AddRGBAColorToDict(CFMutableDictionaryRef objDict, CFStringRef key, CGrgba* color)
{
    CFMutableDictionaryRef colorDict 
	    = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, 
					&kCFTypeDictionaryKeyCallBacks, 
					&kCFTypeDictionaryValueCallBacks);
    AddFloatToDict(colorDict, kKeyRed, color->r);
    AddFloatToDict(colorDict, kKeyGreen, color->g);
    AddFloatToDict(colorDict, kKeyBlue, color->b);
    AddFloatToDict(colorDict, kKeyAlpha, color->a);
    
    CFDictionaryAddValue(objDict, key, colorDict);
    CFRelease(colorDict);
}

//------------------------------------------------------------------------------
void GetRGBAColorFromDict(CFDictionaryRef theDict, CFStringRef key, CGrgba* color)
{
    CFDictionaryRef colorDict = CFDictionaryGetValue(theDict, key);
    color->r = GetFloatFromDict(colorDict, kKeyRed);
    color->g = GetFloatFromDict(colorDict, kKeyGreen);
    color->b = GetFloatFromDict(colorDict, kKeyBlue);
    color->a = GetFloatFromDict(colorDict, kKeyAlpha);
}

/*
//-------------------------------------------------------------- AddPathToDict
// We represent a path as array of path-elements, where each path element is
// a dictionary with a kPathElementType key and up to three points.
static void MyAddToArrayApplier(void *info, const CGPathElement *element)
{
    CFMutableArrayRef array = (CFMutableArrayRef)info;
    CFMutableDictionaryRef pathElementDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, 
					&kCFTypeDictionaryKeyCallBacks, 
					&kCFTypeDictionaryValueCallBacks);
    CGPoint p0, p1, p2;
    
    AddIntegerToDict(pathElementDict, kPathElementType, element->type);

    switch (element->type)
    {
	case kCGPathElementMoveToPoint:
	case kCGPathElementAddLineToPoint:
	    p0 = element->points[0];
	    AddFloatToDict(pathElementDict, kX0, p0.x);
	    AddFloatToDict(pathElementDict, kY0, p0.y);
	// fall through
	
	case kCGPathElementAddQuadCurveToPoint:
	    p1 = element->points[1];
	    AddFloatToDict(pathElementDict, kX1, p1.x);
	    AddFloatToDict(pathElementDict, kY1, p1.y);
	// fall through
	
	case kCGPathElementAddCurveToPoint:
	    p2 = element->points[2];
	    AddFloatToDict(pathElementDict, kX2, p2.x);
	    AddFloatToDict(pathElementDict, kY2, p2.y);
	break;
	
	case kCGPathElementCloseSubpath:    // nothing to do
	break;
    }
    CFArrayAppendValue(array, pathElementDict);
    CFRelease(pathElementDict);
}
    
void AddPathToDict(CFMutableDictionaryRef theDict, CGPathRef path)
{
    CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CGPathApply(path, array, MyAddToArrayApplier);
    CFDictionaryAddValue(theDict, kPath, path);
    CFRelease(array);
}
*/

/*
//------------------------------------------------------------------------------
CGMutablePathRef GetPathFromDict(CFDictionaryRef theDict)
{
    CGMutablePathRef path = CGPathCreateMutable();
    CFArrayRef array = CFDictionaryGetValue(theDict, kPath);
    CFIndex count = CFArrayGetCount(array);
    CGPoint p0, p1, p2;
    CFIndex i;

    for (i = 0; i < count; ++i)
    {
	CFDictionaryRef pathElem = (CFDictionaryRef)CFArrayGetValueAtIndex(array, i);
	int elemType = GetIntegerFromDict(pathElem, kPathElementType);

	if (elemType == kCGPathElementCloseSubpath)
	{
	    CGPathCloseSubpath(path);
	    continue;
	}
	
	p0.x = GetFloatFromDict(pathElem, kX0);
	p0.y = GetFloatFromDict(pathElem, kY0);

	switch (elemType)
	{
	    case kCGPathElementMoveToPoint:
		CGPathMoveToPoint(path, NULL, p0.x, p0.y);
		break;
		
	    case kCGPathElementAddLineToPoint:
		CGPathAddLineToPoint(path, NULL, p0.x, p0.y);
		break;

	    case kCGPathElementAddQuadCurveToPoint:
		p1.x = GetFloatFromDict(pathElem, kX1);
		p1.y = GetFloatFromDict(pathElem, kY1);
		CGPathAddQuadCurveToPoint(path, NULL, p0.x, p0.y, p1.x, p1.y);
		break;
	    
	    case kCGPathElementAddCurveToPoint:
		p1.x = GetFloatFromDict(pathElem, kX1);
		p1.y = GetFloatFromDict(pathElem, kY1);
		p2.x = GetFloatFromDict(pathElem, kX2);
		p2.y = GetFloatFromDict(pathElem, kY2);
		CGPathAddCurveToPoint(path, NULL, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
	}
    }
    
    return path;
}
*/


// More path utilities

//--------------------------------------------------------------
struct PathOffsetStruct {
    CGMutablePathRef newPath;
    float	dx;
    float	dy;
};
typedef struct PathOffsetStruct PathOffsetStruct;

//------------------------------------------------------------------------------
static void MyOffsetApplier(void *info, const CGPathElement *element)
{
    PathOffsetStruct* myStruct = (PathOffsetStruct*)info;
    CGPoint pt, c1, c2;
    
    switch (element->type)
    {
	case kCGPathElementMoveToPoint:
	    pt = element->points[0];
	    pt.x += myStruct->dx;
	    pt.y += myStruct->dy;
	    CGPathMoveToPoint(myStruct->newPath, NULL, pt.x, pt.y);
	break;
	
	case kCGPathElementAddLineToPoint:
	    pt = element->points[0];
	    pt.x += myStruct->dx;
	    pt.y += myStruct->dy;
	    CGPathAddLineToPoint(myStruct->newPath, NULL, pt.x, pt.y);
	break;
	
	case kCGPathElementAddQuadCurveToPoint:
	    c1 = element->points[0];
	    pt = element->points[1];
	    c1.x += myStruct->dx;
	    c1.y += myStruct->dy;
	    pt.x += myStruct->dx;
	    pt.y += myStruct->dy;
	    CGPathAddQuadCurveToPoint(myStruct->newPath, NULL, c1.x, c1.y, pt.x, pt.y);
	break;
	
	case kCGPathElementAddCurveToPoint:
	    c1 = element->points[0];
	    c2 = element->points[1];
	    pt = element->points[2];
	    c1.x += myStruct->dx;
	    c1.y += myStruct->dy;
	    c2.x += myStruct->dx;
	    c2.y += myStruct->dy;
	    pt.x += myStruct->dx;
	    pt.y += myStruct->dy;
	    CGPathAddCurveToPoint(myStruct->newPath, NULL, c1.x, c1.y, c2.x, c2.y, pt.x, pt.y);
	break;
	
	case kCGPathElementCloseSubpath:
	break;
    }
}


CGMutablePathRef CopyPathWithOffset(CGPathRef path, float dx, float dy)
{
    PathOffsetStruct info;
    info.newPath = CGPathCreateMutable();
    info.dx = dx;
    info.dy = dy;
    CGPathApply(path, &info, MyOffsetApplier);
    return info.newPath;
}



//------------------------------------------------------------------------------
#define kNumPointsIncrement	32	/* add room for so many points at a time */
struct PointExtractStruct {
    CGPoint* ptArray;
    ItemCount	numPoints;
    ItemCount	capacity;
};
typedef struct PointExtractStruct PointExtractStruct;

static void 
AddPoints(PointExtractStruct* myStruct, ItemCount numPoints, CGPoint* points)
{
    if (myStruct->numPoints + numPoints > myStruct->capacity)
    {
	myStruct->ptArray = realloc(myStruct->ptArray, myStruct->capacity + kNumPointsIncrement);
	// Error handling ... !!!
	myStruct->capacity += kNumPointsIncrement;
    }
    while (numPoints-- > 0)
    {
	myStruct->ptArray[myStruct->numPoints++] = *points++;
    }
}

//------------------------------------------------------------------------------
static void MyPointExtractApplier(void *info, const CGPathElement *element)
{
    PointExtractStruct* myStruct = (PointExtractStruct*)info;
    switch (element->type)
    {
	case kCGPathElementMoveToPoint:
	    AddPoints(myStruct, 1, element->points);
	break;
	
	case kCGPathElementAddLineToPoint:
	    AddPoints(myStruct, 1, element->points);
	break;
	
	case kCGPathElementAddQuadCurveToPoint:
	    AddPoints(myStruct, 2, element->points);
	break;
	
	case kCGPathElementAddCurveToPoint:
	    AddPoints(myStruct, 3, element->points);
	break;
	
	case kCGPathElementCloseSubpath:
	break;
    }
}

//------------------------------------------------------------------------------
CGPoint* ExtractControlPoints(CGMutablePathRef path, int* outNumPoints)
{
	PointExtractStruct info;
	info.ptArray = (CGPoint*)malloc(sizeof(CGPoint) * kNumPointsIncrement);
	info.numPoints = 0;
	info.capacity = kNumPointsIncrement;
	CGPathApply(path, &info, MyPointExtractApplier);
	*outNumPoints = info.numPoints;
	return info.ptArray;
}

//------------------------------------------------------------------------------
struct ResizePathStruct {
	CGMutablePathRef newPath;
	int		indexOfNewPt;
	int		currentPtIndex;
	CGPoint		newPt;
};
typedef struct ResizePathStruct ResizePathStruct;

static void MyResizePathApplier(void *info, const CGPathElement *element)
{
    ResizePathStruct* myStruct = (ResizePathStruct*)info;
    CGPoint pt, c1, c2;
    
    switch (element->type)
    {
	case kCGPathElementMoveToPoint:
	    pt = element->points[0];
	    myStruct->currentPtIndex += 1;
	    if (myStruct->indexOfNewPt == myStruct->currentPtIndex)
	    {
		pt = myStruct->newPt;
	    }
	    CGPathMoveToPoint(myStruct->newPath, NULL, pt.x, pt.y);
	break;
	
	case kCGPathElementAddLineToPoint:
	    pt = element->points[0];
	    myStruct->currentPtIndex += 1;
	    if (myStruct->indexOfNewPt == myStruct->currentPtIndex)
	    {
		pt = myStruct->newPt;
	    }
	    CGPathAddLineToPoint(myStruct->newPath, NULL, pt.x, pt.y);
	break;
	
	case kCGPathElementAddQuadCurveToPoint:
	    c1 = element->points[0];
	    pt = element->points[1];
	    myStruct->currentPtIndex += 2;
	    if (myStruct->indexOfNewPt == myStruct->currentPtIndex - 1)
		c1 = myStruct->newPt;
	    else if (myStruct->indexOfNewPt == myStruct->currentPtIndex)
		pt = myStruct->newPt;
	    CGPathAddQuadCurveToPoint(myStruct->newPath, NULL, c1.x, c1.y, pt.x, pt.y);
	break;
	
	case kCGPathElementAddCurveToPoint:
	    c1 = element->points[0];
	    c1 = element->points[1];
	    pt = element->points[2];
	    myStruct->currentPtIndex += 3;
	    if (myStruct->indexOfNewPt == myStruct->currentPtIndex - 2)
		c1 = myStruct->newPt;
	    else if (myStruct->indexOfNewPt == myStruct->currentPtIndex - 1)
		c2 = myStruct->newPt;
	    else if (myStruct->indexOfNewPt == myStruct->currentPtIndex)
		pt = myStruct->newPt;
	    CGPathAddCurveToPoint(myStruct->newPath, NULL, c1.x, c1.y, c2.x, c2.y, pt.x, pt.y);
	break;
	
	case kCGPathElementCloseSubpath:
	break;
    }
}

CGMutablePathRef 
CreateResizedPath(CGMutablePathRef oldPath, int ctlPointIndex, CGPoint newPt)
{
    ResizePathStruct info;
    info.newPath = CGPathCreateMutable();
    info.indexOfNewPt = ctlPointIndex;
    info.newPt = newPt;
    info.currentPtIndex = 0;	// control points are 1-based
    CGPathApply(oldPath, &info, MyResizePathApplier);
    return info.newPath;
}


//------------------------------------
/*
void ShowPoint(char* msg, CGPoint pt)
{
    fprintf(stderr, "%s:  (%d, %d)\n", msg, (int)pt.x, (int)pt.y);
}
*/

