/*
    File:       CSkToolPalette.c
        
    Contains:	Implementation of CSkToolPalette creation and event handling.
				
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


#include "CSkToolPalette.h"
#include "CSkConstants.h"
#include "CSkUtils.h"


static WindowRef sToolPaletteWindow = NULL;     // make sure we only make a single one!

// ToolPaletteStorage holds the state of whatever the ToolPalette reflects:
// currently selected tool, line drawing specs, and stroke and fill color
// (including alpha: see CGrgba in CSkUtils.h).

struct ToolPaletteStorage	
{
    int   selectedTool;
    CSkObjectAttributes attr;
};
typedef struct ToolPaletteStorage ToolPaletteStorage;

//------------------------------------------------------------------------
static ToolPaletteStorage* CreateToolPaletteStorage()
{
    const RGBColor rgbBlack = { 0, 0, 0 };
    const RGBColor rgbWhite = { 0xFFFF, 0xFFFF, 0xFFFF };
    
    ToolPaletteStorage* tPS = (ToolPaletteStorage*)NewPtrClear( sizeof(ToolPaletteStorage) );
    
    // Set defaults in tPS - these should match the checked menu items in the Nib file
    
    tPS->selectedTool		= kLineTool;
    tPS->attr.lineWidth		= 4.0;
    tPS->attr.lineCap		= kCGLineCapButt;
    tPS->attr.lineJoin		= kCGLineJoinMiter;
    tPS->attr.lineStyle		= kStyleSolid;
    ConvertRGBColorToCGrgba(&rgbBlack, 1.0, &tPS->attr.strokeColor);
    ConvertRGBColorToCGrgba(&rgbWhite, 1.0, &tPS->attr.fillColor);

    return tPS;
}


//-------------------------------------------------------------------- Accessors
int CSkToolPaletteGetTool(WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return tps->selectedTool;
}

CSkObjectAttributes* CSkToolPaletteGetAttributes( WindowRef toolPalette )
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return &tps->attr;
}

float CSkToolPaletteGetLineWidth(WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return tps->attr.lineWidth;
}

CGLineCap CSkToolPaletteGetLineCap(WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return tps->attr.lineCap;
}

CGLineJoin CSkToolPaletteGetLineJoin(WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return tps->attr.lineJoin;
}

int CSkToolPaletteGetLineStyle(WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    return tps->attr.lineStyle;
}

CGrgba* CSkToolPaletteGetStrokeColor( WindowRef toolPalette, CGrgba* outStrokeColor )
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    *outStrokeColor = tps->attr.strokeColor;
    return outStrokeColor;
}

void CSkToolPaletteSetStrokeColor( WindowRef toolPalette, CGrgba* strokeColor )
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    tps->attr.strokeColor = *strokeColor;
}

CGrgba* CSkToolPaletteGetFillColor( WindowRef toolPalette, CGrgba* outFillColor )
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    *outFillColor = tps->attr.fillColor;
    return outFillColor;
}

void CSkToolPaletteSetFillColor( WindowRef toolPalette, CGrgba* fillColor )
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    tps->attr.fillColor = *fillColor;
}

void CSkToolPaletteSetContextState( CGContextRef ctx, WindowRef toolPalette)
{
    ToolPaletteStorage* tps = (ToolPaletteStorage*)GetWRefCon(toolPalette);
    CGContextSetLineWidth(ctx, tps->attr.lineWidth);
    CGContextSetLineCap(ctx, tps->attr.lineCap);
    CGContextSetLineJoin(ctx, tps->attr.lineJoin);
    CGContextSetStrokeColor( ctx, (CGFloat*)&(tps->attr.strokeColor));
    CGContextSetFillColor( ctx, (CGFloat*)&(tps->attr.fillColor));
}

//-------------------------------------------------
// One of those spots where the code needs to be in sync with the design in the NIB:
static int LineWidthToMenuItem(float lineWidth)
{
    const float widths[] = { 0, 1, 2, 4, 8, 16 };
    int i = 0;
    do {
        if (widths[i] == lineWidth)
            return (i + 1);         // itemNo is 1-based
        i += 1;
    } while (i < 6);
    
    return 0;   // no match
}

//-----------------------------------------------------------------------------------------------------------------------
// Each time the selected object changes, the ToolPalette needs to reflect the current state of the
// selected object. This is accomplished in the "CSkToolPaletteSyncPopups" routine, below.

static void InternalSyncPopups(const CSkObjectAttributes* attr)
{
    ControlID   cntlID      = { kControlSignaturePalette, kLineWidthPopup };
    short       itemNo      = LineWidthToMenuItem(attr->lineWidth);
    ControlRef  control;
    
    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlData(control, kControlEntireControl, kControlBevelButtonMenuValueTag, sizeof(itemNo), &itemNo);
    
    cntlID.id = kLineCapPopup;
    itemNo = (short)attr->lineCap + 1;
    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlData( control, kControlEntireControl, kControlBevelButtonMenuValueTag, sizeof(itemNo), &itemNo);

    cntlID.id = kLineJoinPopup;
    itemNo = (short)attr->lineJoin + 1;
    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlData( control, kControlEntireControl, kControlBevelButtonMenuValueTag, sizeof(itemNo), &itemNo);
    
    cntlID.id = kLineStylePopup;
    itemNo = (short)attr->lineStyle;
    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlData( control, kControlEntireControl, kControlBevelButtonMenuValueTag, sizeof(itemNo), &itemNo);
}

static void SyncAlphaSliders(float strokeAlpha, float fillAlpha)
{
    ControlID   cntlID      = { kControlSignaturePalette, kStrokeAlphaSlider };
    ControlRef  control;

    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlValue(control, 100 * strokeAlpha);
    
    cntlID.id = kFillAlphaSlider;
    GetControlByID( sToolPaletteWindow, &cntlID, &control );
    SetControlValue(control, 100 * fillAlpha);
}

void CSkToolPaletteSyncPopups (CSkObject* obj)
{
    if (obj != NULL)
    {
        InternalSyncPopups(CSkObjectGetAttributes(obj));
		SyncAlphaSliders(GetStrokeAlpha(obj), GetFillAlpha(obj));
    }
}

//-----------------------------------------------------------------------------------------------------------------------
// We want our sliders for stroke alpha and fill alpha and scale to provide immediate feedback. 
// Sending a corresponding HICommand to the frontmost document window from within the ActionProc does just that.

static void SliderControlActionProc( ControlRef control, ControlPartCode partCode )
{
#pragma unused(partCode)
    ControlID       cntlID;
    HICommand       cmd;

    memset(&cmd, 0, sizeof(HICommand));
    
    GetControlID(control, &cntlID);
    if (cntlID.id == kStrokeAlphaSlider)
    {
        cmd.commandID = kCmdStrokeAlphaChanged;
    }
    else if (cntlID.id == kFillAlphaSlider)
    {
        cmd.commandID = kCmdFillAlphaChanged;
    }
	else if (cntlID.id == kScaleSlider)
	{
        cmd.commandID = kCmdScalingValueChanged;
	}
	
    if (cmd.commandID != 0)
    {
        WindowRef window = FrontNonFloatingWindow();	
        // if NULL, we should have closed the ToolPalette, or at least disabled its controls!
        if ( window != NULL)
        {
            SendWindowCommandEvent(window, &cmd);
        }
    }
}

//-----------------------------------------------------------------------------------------------------------------------
// Our ToolPaletteEventHandler needs to respond to all the HICommands sent by the various controls,
// deal specially with the "alpha" sliders, and hook into kEventWindowClose to deallocate the toolPStorage.

static pascal OSStatus ToolPaletteEventHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    #pragma unused ( inCallRef )
    OSStatus	err		= eventNotHandledErr;
    UInt32	eventKind   = GetEventKind( inEvent );
    UInt32	eventClass  = GetEventClass( inEvent );

    WindowRef               toolPalette  = (WindowRef)inUserData;
    ToolPaletteStorage*     toolPStorage = (ToolPaletteStorage*)GetWRefCon( toolPalette );
        
    switch ( eventClass )
    {
    case kEventClassWindow:
        if ( eventKind == kEventWindowClose )	// Dispose extra window storage here
        {
            DisposePtr( (Ptr) toolPStorage );
            SetWRefCon(toolPalette, 0);
        }
        break;
        
    case kEventClassCommand:
        if ( eventKind == kEventCommandProcess )
        {
            HICommandExtended   command;
            HICommand           windowCmd;
            
            memset(&windowCmd, 0, sizeof(HICommand));
			
            GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command );
            switch ( command.commandID )
            {
                // Line Width
                case kCmdWidthNone:      
                        toolPStorage->attr.lineWidth = 0;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthOne:       
                        toolPStorage->attr.lineWidth = 1;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthTwo:       
                        toolPStorage->attr.lineWidth = 2;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthFour:      
                        toolPStorage->attr.lineWidth = 4;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthEight:     
                        toolPStorage->attr.lineWidth = 8;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthSixteen:   
                        toolPStorage->attr.lineWidth = 16;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthThinner:   
                        if (toolPStorage->attr.lineWidth > 0)
                                toolPStorage->attr.lineWidth -= 1;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;
                case kCmdWidthThicker:
                        toolPStorage->attr.lineWidth += 1;
                        windowCmd.commandID = kCmdLineWidthChanged;
                        break;

                // Line Cap
                case kCmdCapButt:        
                        toolPStorage->attr.lineCap = kCGLineCapButt;
                        windowCmd.commandID = kCmdLineCapChanged;
                        break;
                case kCmdCapRound:       
                        toolPStorage->attr.lineCap = kCGLineCapRound;
                        windowCmd.commandID = kCmdLineCapChanged;
                        break;
                case kCmdCapSquare:      
                        toolPStorage->attr.lineCap = kCGLineCapSquare;
                        windowCmd.commandID = kCmdLineCapChanged;
                        break;

                // Line Join
                case kCmdJoinMiter:
                        toolPStorage->attr.lineJoin = kCGLineJoinMiter;
                        windowCmd.commandID = kCmdLineJoinChanged;
                        break;
                case kCmdJoinRound:
                        toolPStorage->attr.lineJoin = kCGLineJoinRound;
                        windowCmd.commandID = kCmdLineJoinChanged;
                        break;
                case kCmdJoinBevel:
                        toolPStorage->attr.lineJoin = kCGLineJoinBevel;
                        windowCmd.commandID = kCmdLineJoinChanged;
                        break;

                // Line Style
                case kCmdStyleSolid:
                        toolPStorage->attr.lineStyle = kStyleSolid;
                        windowCmd.commandID = kCmdLineStyleChanged;
                        break;
                case kCmdStyleDashed:
                        toolPStorage->attr.lineStyle = kStyleDashed;
                        windowCmd.commandID = kCmdLineStyleChanged;
                        break;

                // Color selection
                case kCmdStrokeColor:    
                        PickSomeColor(&toolPStorage->attr.strokeColor);
                        windowCmd.commandID = kCmdStrokeColorChanged;
                        break;

                case kCmdFillColor:      
                        PickSomeColor(&toolPStorage->attr.fillColor);
                        windowCmd.commandID = kCmdFillColorChanged;
                        break;
                        
                default:
                {   
//                  char* c = (char*)&command.commandID;
//                  fprintf(stderr, "ToolPaletteCommand kEventCommandProcess ' %c%c%c%c'\n", c[0], c[1], c[2], c[3]);
                }
            }
               
            if (windowCmd.commandID != 0)
            {
                WindowRef window = FrontNonFloatingWindow();	
                // if NULL, we should have closed the ToolPalette, or at least disabled its controls!
                if ( window != NULL)
                {
                    SendWindowCommandEvent(window, &windowCmd);
                }
            }
        }
        else if (eventKind == kHICommandClose)
        {
            DisposePtr((Ptr)toolPStorage);
            SetWRefCon(toolPalette, 0);
//          fprintf(stderr, "ToolPalette: closed via kEventClassCommand/kHICommandClose\n");
        }
        break;
    }
	
    return err;
}

//-----------------------------------------------------------------------------------------------------------------------
// This ControlEventHandler is installed on the buttons to select the current "Tool"
static pascal OSStatus ControlEventHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
//  UInt32      eventClass  = GetEventClass(inEvent);
    UInt32      eventKind   = GetEventKind(inEvent);
    OSStatus    err         = eventNotHandledErr;
    
    if ( eventKind == kEventControlHit)
    {
        ControlRef          control         = (ControlRef)inUserData;
        WindowRef           wRef            = GetControlOwner(control);
        ToolPaletteStorage* toolPStorage    = (ToolPaletteStorage*)GetWRefCon(wRef);
        ControlID           cntlID;
        int                 i;
        
        GetControlID(control, &cntlID);

        if ((cntlID.id >= kFirstTool) && (cntlID.id <= kLastTool))
        {
            toolPStorage->selectedTool = cntlID.id;
        
            // Deselect old tool, select new tool
        
            for (i = kFirstTool; i <= kLastTool; ++i)
            {                
                int cntlValue = (i == toolPStorage->selectedTool ? 1 : 0);
                
                cntlID.id = i;      // cntlID.signature = 'Tool', from above
                err = GetControlByID( wRef, &cntlID, &control );
                require_noerr( err, CantGetControlByID );
				SetControl32BitValue(control, cntlValue);
            }
        }
    }

CantGetControlByID:            
    err = CallNextEventHandler( inCallRef, inEvent );
    return err;
}


//------------------------------------------------------------------------
WindowRef CSkToolPalette( IBNibRef theNibRef )
{
    static EventHandlerUPP  gToolPaletteEventProc   = NULL;
	static EventHandlerUPP  gControlEventProc		= NULL;
	static ControlActionUPP	sliderControlActionUPP  = NULL;
	
    if (sToolPaletteWindow == NULL)
    {
        const EventTypeSpec toolPaletteEvents[] =   {   { kEventClassCommand, kEventCommandProcess },
                                                        { kEventClassWindow, kEventWindowClose }
                                                    };
                                                    
        const EventTypeSpec controlEventList[] =    {   { kEventClassControl, kEventControlHit }
                                                    };
        
        ToolPaletteStorage* tPS;
        OSStatus            err;
        
        err = CreateWindowFromNib( theNibRef, CFSTR("ToolPalette"), &sToolPaletteWindow );
        if (err == noErr)
        {
            if (gToolPaletteEventProc == NULL)
			{
				gToolPaletteEventProc = NewEventHandlerUPP(ToolPaletteEventHandler);
				if (gToolPaletteEventProc == NULL)
					return NULL;		// Why do we bother?
			}
			
			tPS = CreateToolPaletteStorage();
            SetWRefCon( sToolPaletteWindow, (SRefCon)tPS );
            
            err = InstallWindowEventHandler (   sToolPaletteWindow, 
                                                gToolPaletteEventProc, 
                                                GetEventTypeCount(toolPaletteEvents), 
                                                toolPaletteEvents, 
                                                sToolPaletteWindow, 
                                                NULL );
            require_noerr( err, CantInstallWindowEventHandler );

            // Install our ControlEventHandler on Tool buttons
            {
                ControlID   cntlID      = { kControlSignaturePalette, tPS->selectedTool };
                ControlRef  control;
                int         i;
                
				if (gControlEventProc == NULL)
				{
					gControlEventProc = NewEventHandlerUPP(ControlEventHandler);
					if (gControlEventProc == NULL)
						return NULL;		// Why do we bother?
				}
				
                err = GetControlByID( sToolPaletteWindow, &cntlID, &control );
                SetControlValue(control, 1);  

                for (i = kFirstTool; i <= kLastTool; ++i)
                {
                    cntlID.id = i;
                    
                    err = GetControlByID( sToolPaletteWindow, &cntlID, &control );
                    require_noerr( err, CantGetControlByID );
                    
                    err = InstallControlEventHandler(   control, 
                                                        gControlEventProc,
                                                        GetEventTypeCount( controlEventList ), 
                                                        controlEventList, 
                                                        control, 
                                                        NULL );
                    require_noerr( err, CantInstallControlEventHandler );
                }

   				if ( sliderControlActionUPP == NULL )
					sliderControlActionUPP  = NewControlActionUPP( SliderControlActionProc );
				
				// Set up the sliders
				for (i = kStrokeAlphaSlider; i <= kScaleSlider; ++i)
				{
					cntlID.id = i;
					err	= GetControlByID( sToolPaletteWindow, &cntlID, &control );	
					require_noerr( err, CantInstallControlEventHandler );	
					SetControlAction( control, sliderControlActionUPP );
				}

				// Set up the bevel button min/max values according to their menu items
                cntlID.id = kLineWidthPopup;
                GetControlByID( sToolPaletteWindow, &cntlID, &control );
                SetControlMinimum(control, 0);
                SetControlMaximum(control, kLastLineWidthItem);
                
                cntlID.id = kLineCapPopup;
                GetControlByID( sToolPaletteWindow, &cntlID, &control );
                SetControlMinimum(control, 0);
                SetControlMaximum(control, 3);

                cntlID.id = kLineJoinPopup;
                GetControlByID( sToolPaletteWindow, &cntlID, &control );
                SetControlMinimum(control, 0);
                SetControlMaximum(control, 3);

                cntlID.id = kLineStylePopup;
                GetControlByID( sToolPaletteWindow, &cntlID, &control );
                SetControlMinimum(control, 0);
                SetControlMaximum(control, 2);

                InternalSyncPopups(&tPS->attr);
           }
            
    CantInstallWindowEventHandler:
    CantGetControlByID:
    CantInstallControlEventHandler:
            ShowWindow( sToolPaletteWindow );
        }
    }
    return sToolPaletteWindow;
}
