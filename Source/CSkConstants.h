/*
    File:       CSkConstants.h
        
    Contains:	Application-wide constant definitions.

    Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                ("Apple") in consideration of your agreement to the following terms, and your
                use, installation, modification or redistribution of this Apple software
                constitutes acceptance of these terms.  If you do not agree with these terms,
                please do not use, install, modify or redistribute this Apple software.

                In consideration of your agreement to abide by the following terms, and subject
                to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
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


#ifndef __CSKCONSTANTS__
#define __CSKCONSTANTS__

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

// We require 10.2. Certain APIs only are available on Pather (10.3) or Tiger (10.4).
// We set the corresponding globals at startup (in main.c).

extern Boolean gOnTiger;
extern Boolean gOnPanther;

enum {
    kCSkSignature		= 'CSkh',
    kCSkPerWindowStorage	= 'WStg'
};


enum {
    kUnsupportedFileFormat	= 1    // error code range???
};

// Window controls

enum { 
    kDocumentViewSignature	= 'CskV',
    kDocumentBoundsParam	= 'Boun',
    kCmdChangeDocumentSize	= 'CDCs'
 };

enum {
    kPWStaticID1		= 10,	// "The PDF is password protected."
    kPWStaticID2		= 11,	// "Please type the password below."
    kPWStaticID3		= 12,	// "Password:"
    kPWStaticID4		= 13	// "Invalid password"
};

enum {
    kScrollbarWidth		= 15
};

enum {
    kControlSignatureMainWindow = 'CSkM',
    kScrollHorizontalID		= 1,
    kScrollVerticalID		= 2
};

enum {
    kMinimumScaleCtlValue	= 0,	// see CSkWindow.c, ScaleCtlValueToScalingFactor about how to convert
    kMaximumScaleCtlValue	= 80
};


// default document size and position in window
enum {
    kDefaultDocWidth		= 576,
    kDefaultDocHeight		= 720,
    kLeftMargin			= 18,	// 0.25 inch
    kTopMargin			= 18,
    kGridWidth			= 18
};
    
// Geometry (shape) selectors
enum {
    kUndefined			= 0,
    kLineShape			= 1,	// Values of constants used in program logic:
    kQuadBezier			= 2,	// Don't change these 1 - 2 - 3 values!
    kCubicBezier		= 3,	// "
    kRectShape			= 4,
    kOvalShape			= 5,
    kRRectShape			= 6,
    kFreePolygon		= 7,	// as opposed to regular polygons
    // and so on ...
};

//---------------------------------------------------------------------------------------------
//	Floating palette buttons - cf. CSkUtils, MapToolToShape()
//---------------------------------------------------------------------------------------------

enum {
    kControlSignaturePalette	= 'Tool',
    kArrowTool			= 1,
    kFirstTool			= kArrowTool,
    kLineTool			= 2,
    kRectTool			= 3,
    kOvalTool			= 4,
    kRRectTool			= 5,
    kPolyTool			= 6,
    kQuadTool			= 7,
    kBezierTool			= 8,
    kLastTool			= kBezierTool,
    
    kLineWidthPopup		= 10,
    kLineCapPopup		= 11,
    kLineJoinPopup		= 12,
    kLineStylePopup		= 13,
    
    kStrokeColorBtn		= 20,
    kFillColorBtn		= 21,
    
    // keep the following numbers consecutive 
    kStrokeAlphaSlider		= 30,
    kFillAlphaSlider		= 31,
    kScaleSlider		= 32
};

// Line Width menu items
enum {
    kCmdWidthNone		= 'WdNn',
    kCmdWidthOne		= 'Wd01',
    kCmdWidthTwo		= 'Wd02',
    kCmdWidthFour		= 'Wd04',
    kCmdWidthEight		= 'Wd08',
    kCmdWidthSixteen		= 'Wd16',
    kCmdWidthThinner		= 'Wd< ',
    kCmdWidthThicker		= 'Wd> ',
    kMakeItThinner		= -2,
    kMakeItThicker		= -1,
    kLastLineWidthItem		= 8,
    kCmdStrokeColor		= 'StrC',
    kCmdFillColor		= 'FilC'
};

// Line Cap commandIDs
enum {
    kCmdCapButt			= 'Cap1',
    kCmdCapRound		= 'Cap2',
    kCmdCapSquare		= 'Cap3'
};


// Line Join menu items
enum {
    kCmdJoinMiter		= 'Jon1',
    kCmdJoinRound		= 'Jon2',
    kCmdJoinBevel		= 'Jon3'
};

// Line Style menu items
enum
{
    kStyleSolid			= 1,
    kStyleDashed		= 2,
    kCmdStyleSolid		= 'Stl1',
    kCmdStyleDashed		= 'Stl2'
};

// Arrange menu item commands
enum 
{
    kCmdMoveForward		= 'Fwrd',
    kCmdMoveToFront		= 'Frnt',
    kCmdMoveBackward		= 'Bwrd',
    kCmdMoveToBack		= 'Back'
};

// Other menu or window commands
enum
{   
    kCmdWritePDF		= 'WPDF',
    kCmdDuplicate		= 'Dupl',
    kCmdLineWidthChanged	= 'LwCh',
    kCmdLineCapChanged		= 'LcCh',
    kCmdLineJoinChanged		= 'LjCh',
    kCmdLineStyleChanged	= 'LsCh',
    kCmdStrokeColorChanged	= 'SCCh',
    kCmdFillColorChanged	= 'FCCh',
    kCmdStrokeAlphaChanged	= 'Ssld',
    kCmdFillAlphaChanged	= 'Fsld',
    kCmdScalingValueChanged	= 'Scal',
    kCmdNextPageOrImageIndex	= 'Next',
    kCmdPrevPageorImageIndex	= 'Prev',
    kCmdFirstPageOrImageIndex	= 'Frst',
    kCmdLastPageOrImageIndex	= 'Last'
};

// Keys for object dictionary

#define kX0		    CFSTR("x0")
#define kY0		    CFSTR("y0")
#define kX1		    CFSTR("x1")
#define kY1		    CFSTR("y1")
#define kX2		    CFSTR("x2")
#define kY2		    CFSTR("y2")
#define kX3		    CFSTR("x3")
#define kY3		    CFSTR("y3")
#define krX		    CFSTR("rX")
#define krY		    CFSTR("rY")
#define kPath		    CFSTR("path")
#define	kPathElementType    CFSTR("pathElementType")
#define kKeyShapeType	    CFSTR("shapeType")
#define kKeyLineWidth	    CFSTR("lineWidth")
#define kKeyLineCap	    CFSTR("lineCap")
#define kKeyLineJoin	    CFSTR("lineJoin")
#define kKeyLineStyle	    CFSTR("lineStyle")
#define kKeyStrokeColor	    CFSTR("strokeColor")
#define kKeyFillColor	    CFSTR("fillColor")
#define kKeyRed		    CFSTR("red")
#define kKeyGreen	    CFSTR("green")
#define kKeyBlue	    CFSTR("blue")
#define kKeyAlpha	    CFSTR("alpha")
#define kKeyObjectArray	    CFSTR("objArray")

#endif
