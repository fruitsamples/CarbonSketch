/*
    File:       CSkWindow.c
        
    Contains:	Implementation of CSkWindow creation and event handling.
				
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

#include "CSkWindow.h"
#include "CSkDocStorage.h"
#include "CSkDocumentView.h"
#include "CSkConstants.h"
#include "CSkUtils.h"
#include "CSkObjects.h"
#include "CSkToolPalette.h"
#include "CSkPrinting.h"
#include "NavServicesHandling.h"
#include "CSkDocumentView.h"
#include "CSkPDFPasswordEntry.h"


//-----------------------------------------------------------------------------------------------------------------------
static OSStatus UpdateOverlayWindow(DocStorage* inDocStP, HIViewRef inView, WindowRef* outOverlayWindow, HIViewRef* outOverlayView)
{
    HIRect	frame;
    Rect	wBounds;
    UInt32	flags = kWindowHideOnSuspendAttribute | kWindowIgnoreClicksAttribute | kWindowCompositingAttribute;
    HIViewRef	view;
    HIViewRef	content;
    const EventTypeSpec	kOverlayViewEvents[] =
    {
	{ kEventClassControl, kEventControlDraw }
    };

    if (*outOverlayWindow != NULL)
	DisposeWindow(*outOverlayWindow);
    
    // We need to pass the frame of the HIView in global coordinates to CreateNewWindow
    HIViewGetFrame(inView, &frame);
    if (gOnTiger)	// HIRectConvert is 10.4 only
    {
	HIRectConvert(&frame, kHICoordSpaceView, inView, kHICoordSpace72DPIGlobal, NULL);
    }
    else
    {
	GetWindowBounds(GetControlOwner(inView), kWindowStructureRgn, &wBounds);	// wBounds is in global coordinates
	// Make frame global
	frame.origin.x += wBounds.left;
	frame.origin.y += wBounds.top;
    }
    
    // Reuse wBounds to create the overlay window
    wBounds.top = frame.origin.y;
    wBounds.left = frame.origin.x;
    wBounds.bottom = wBounds.top + frame.size.height;
    wBounds.right = wBounds.left + frame.size.width;
	
    OSStatus err = CreateNewWindow( kOverlayWindowClass, flags, &wBounds, outOverlayWindow );
    require_noerr(err, CreateNewWindowFAILED);
    
    // put a generic HIView into the window, and use it to draw our overlay content
    err = HIObjectCreate( kHIViewClassID, NULL, (HIObjectRef*) &view );
    require_noerr( err, CreateHIViewFAILED );
    
    *outOverlayView = view;
    
    frame = CGRectOffset( frame, -frame.origin.x, -frame.origin.y );
    HIViewSetFrame( view, &frame );
    HIViewSetVisible( view, true );
    HIViewFindByID( HIViewGetRoot( *outOverlayWindow ), kHIViewWindowContentID, &content );
    HIViewAddSubview( content, view );
    HIViewInstallEventHandler( view, OverlayViewHandler, GetEventTypeCount( kOverlayViewEvents ), kOverlayViewEvents, inDocStP, NULL );

CreateHIViewFAILED:
CreateNewWindowFAILED:
    return err;
}   // UpdateOverlayWindow

//--------------------------------------------------------------------------------------------------
/////////////////////////////// Support for Copy/Paste of PDF Data /////////////////////////////////
//--------------------------------------------------------------------------------------------------

// To create PDF data for the Pasteboard, we need to set up a CFDataConsumer that collects data in a CFMutableDataRef.
// Here are the two required callbacks:

static size_t MyCFDataPutBytes(void* info, const void* buffer, size_t count)
{
	CFDataAppendBytes((CFMutableDataRef)info, buffer, count);
	return count;
}

static void MyCFDataRelease(void* info)
{
	CFRelease((CFMutableDataRef)info);
}

//--------------------------------------------------------------------------------------------------
// Add the windows contents to the Pasteboard as PDF.  This function assumes that the pasteboard is
// not NULL. Also, note that we don't include a pdf background if it is password-protected.
static OSStatus 
AddWindowContentToPasteboardAsPDF( PasteboardRef pasteboard, DocStorage* docStP)
{
    OSStatus		err		= noErr;
    CFDataRef		pdfData		= CFDataCreateMutable( kCFAllocatorDefault, 0);
    CGContextRef	pdfContext;
    CGDataConsumerRef	consumer;
    CGDataConsumerCallbacks cfDataCallbacks = { MyCFDataPutBytes, MyCFDataRelease };
    
    // We need to clear the pasteboard of it's current contents so that this application can
    // own it and add it's own data.
    err = PasteboardClear( pasteboard );
    require_noerr( err, PasteboardClear_FAILED );

    consumer = CGDataConsumerCreate((void*)pdfData, &cfDataCallbacks);
    
    // For now (and for demo purposes), just put the whole window drawing as pdf data
    // on the paste board, regardless of what is selected in the window.
    pdfContext = CGPDFContextCreate(consumer, &docStP->pageRect, NULL);
    require(pdfContext != NULL, CGPDFContextCreate_FAILED);
    
    CGContextBeginPage(pdfContext, &docStP->pageRect);
    // Note that we never get here with pdfIsProtected = true && pdfIsUnlocked = false!
    if (docStP->pdfIsProtected)
	    docStP->pdfIsUnlocked = false;	// pretend it's not unlocked, so it doesn't get drawn
    DrawThePage(pdfContext, docStP);
    if (docStP->pdfIsProtected)
	    docStP->pdfIsUnlocked = true;	
    CGContextEndPage(pdfContext);
    CGContextRelease(pdfContext);   // this finalizes the pdfData
    
    err = PasteboardPutItemFlavor( pasteboard, (PasteboardItemID)1,
					    CFSTR("com.adobe.pdf"), pdfData, 0 );
    require_noerr( err, PasteboardPutItemFlavor_FAILED );
    
CGPDFContextCreate_FAILED:
PasteboardPutItemFlavor_FAILED:
    CGDataConsumerRelease(consumer);	// this also releases the pdfData, via MyCFDataRelease

PasteboardClear_FAILED:
    return err;
}   // AddWindowContentToPasteboardAsPDF

//--------------------------------------------------------------------------------------------------
// Check whether the pasteboard contains pdf data. 
// If so, return the CFDataRef in the pdfData parameter, if it's not NULL.
static Boolean PasteboardContainsPDF(PasteboardRef inPasteboard, CFDataRef* pdfData)
{
    Boolean		gotPDF	    = false;
    OSStatus		err	    = noErr;
    PasteboardSyncFlags syncFlags;
    ItemCount		itemCount;
    UInt32		itemIndex;
    
    if (inPasteboard == NULL)	// only if GetPasteboard() failed to allocate the Pasteboard!
    {
	return false;
    }
    
    syncFlags = PasteboardSynchronize(inPasteboard);

    if (syncFlags & kPasteboardModified)
    {
	fprintf(stderr, "Pasteboard modified\n");
    }
    
    // Count the number of items on the pasteboard so we can iterate through them.
    err = PasteboardGetItemCount(inPasteboard, &itemCount);
    require_noerr(err, PasteboardGetItemCount_FAILED);
    
    for (itemIndex = 1; itemIndex <= itemCount; ++itemIndex)
    {
	PasteboardItemID	itemID;
	CFArrayRef			flavorTypeArray;
	CFIndex				flavorCount;
	CFIndex				flavorIndex;
	
	// Every item is identified by a unique value.
	err = PasteboardGetItemIdentifier( inPasteboard, itemIndex, &itemID );
	require_noerr( err, PasteboardGetItemIdentifier_FAILED );
	
	// The flavorTypeArray is a CFType and we'll need to call CFRelease on it later.
	err = PasteboardCopyItemFlavors( inPasteboard, itemID, &flavorTypeArray );
	require_noerr( err, PasteboardCopyItemFlavors_FAILED );
	
	flavorCount = CFArrayGetCount( flavorTypeArray );
	
	for (flavorIndex = 0; flavorIndex < flavorCount; ++flavorIndex)
	{
	    CFStringRef		flavorType;
	    CFComparisonResult	comparisonResult;
	    
	    // grab the flavorType so we can extract it's flags and data
	    flavorType = (CFStringRef)CFArrayGetValueAtIndex( flavorTypeArray, flavorIndex );

	    // Don't care about flavorFlags, for now
	    {
    //		PasteboardFlavorFlags	flavorFlags;
    //		err = PasteboardGetItemFlavorFlags( inPasteboard, itemID, flavorType, &flavorFlags );
    //		require_noerr( err, PasteboardGetItemFlavorFlags_FAILED );
	    }

	    // Look at the flavorTypeStr for debugging:
	    {
    //		char flavorTypeStr[128];
    //		CFStringGetCString( flavorType, flavorTypeStr, 128, kCFStringEncodingMacRoman );
    //		fprintf(stderr, "%s\n", flavorTypeStr);
	    }
	    
	    // If it's a pdf, get it and return true
//	    comparisonResult = CFStringCompare(flavorType, CFSTR("com.adobe.pdf"), 0);
	    comparisonResult = CFStringCompare(flavorType, kUTTypePDF, 0);
	    if (comparisonResult == kCFCompareEqualTo)
	    {
		if (pdfData != NULL)
		{
		    err = PasteboardCopyItemFlavorData( inPasteboard, itemID, flavorType, pdfData );
		    require_noerr( err, PasteboardCopyItemFlavorData_FAILED );
		}
		gotPDF = true;
		break;
	    }
				    
    PasteboardCopyItemFlavorData_FAILED:
    //PasteboardGetItemFlavorFlags_FAILED:
	    ;
	}
	
	CFRelease(flavorTypeArray);

PasteboardCopyItemFlavors_FAILED:
PasteboardGetItemIdentifier_FAILED:
	;
    }
	
PasteboardGetItemCount_FAILED:	
    return gotPDF;
}   // PasteboardContainsPDF


//--------------------------------------------------------------------------------------------------
// We keep the pasted-in pdf data separately from the CSkObject list.
// A new "Open PDF ..." or "Paste" replaces any previous pdf data.
// If the PDF is password protected, try to unlock it via a password entry dialog.
void AttachPDFToWindow(WindowRef w, CFDataRef pdfData)
{
    DocStorage*	docStP = GetWindowDocStoragePtr(w);
    if (docStP != NULL)
    {
	if (docStP->pdfData != NULL)
		CFRelease(docStP->pdfData);
	if (docStP->pdfDocument != NULL)
		CGPDFDocumentRelease(docStP->pdfDocument);
		
	docStP->pdfData = CFRetain(pdfData);
	CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, CFDataGetBytePtr(pdfData), CFDataGetLength(pdfData), NULL);
	docStP->pdfDocument = CGPDFDocumentCreateWithProvider(provider);
	CFRelease(provider);	// we created it
	
	// Now check whether the pdf is password protected, and if so, try to unlock it
	docStP->pdfIsProtected  = false; // by default (when CGPDFDocumentIsEncrypted() returns false)
	docStP->pdfIsUnlocked = true;

	if ( CGPDFDocumentIsEncrypted(docStP->pdfDocument) )
	{
	    docStP->pdfIsProtected  = true;
	    docStP->pdfIsUnlocked = UnlockPDFDocument(docStP->pdfDocument);
	}
	
	docStP->indexOrPageNo = 1;
    }
}	// AttachPDFToWindow


//-------------------------------------------------------
static void AttachCGImageSourceToWindow(WindowRef w, CGImageSourceRef imgSrc)
{
    DocStorage*	docStP = GetWindowDocStoragePtr(w);
    if (docStP != NULL)
    {
	if (docStP->pdfData != NULL)
	{
	    CFRelease(docStP->pdfData);
	    docStP->pdfData = NULL;
	}
	if (docStP->pdfDocument != NULL)
	{
	    CGPDFDocumentRelease(docStP->pdfDocument);
	    docStP->pdfDocument = NULL;
	}
	if (docStP->cgImgSrc != NULL)
	{
	    CFRelease(docStP->cgImgSrc);
	}
	docStP->cgImgSrc = (CGImageSourceRef)CFRetain(imgSrc);
	docStP->indexOrPageNo = 1;
    }
}

//-------------------------------------------------------
static OSStatus OpenFileForWindow(WindowRef w, FSRef* fsRef)
{
    CFURLRef		url = CFURLCreateFromFSRef(NULL, fsRef);
    LSItemInfoRecord    info;
    OSStatus		err;
    
    err = LSCopyItemInfoForURL( url, kLSRequestExtension | kLSRequestTypeCreator, &info );
    
    if ( info.extension != NULL )
    {
	// If a pdf document, attach it to window
	if (CFStringCompare(info.extension, kUTTypePDF, 0) == kCFCompareEqualTo)
	{
	    CFDataRef data;
	    if (!CFURLCreateDataAndPropertiesFromResource(NULL, url, &data, NULL, NULL, &err))
	    {
		fprintf(stderr, "CFURLCreateDataAndPropertiesFromResource returned %d\n", (int)err);
		return err;
	    }
	    AttachPDFToWindow(w, data);
	}
	else if (CFStringCompare(info.extension, CFSTR("CSk "), 0) == kCFCompareEqualTo)
	{
	    CFStringRef fileName = CFURLCopyLastPathComponent(url);
	    SetWindowTitleWithCFString(w, fileName);
	    CFRelease(fileName);

	    CFReadStreamRef readStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
	    CFStringRef errorString = NULL;
	    /* Boolean success = */ CFReadStreamOpen(readStream);
	    CFPropertyListFormat format;
	    CFPropertyListRef propList = CFPropertyListCreateFromStream(kCFAllocatorDefault, readStream, 0, 
			    kCFPropertyListMutableContainers, &format, &errorString);
	    CFReadStreamClose(readStream);
	    if (errorString != NULL)
		CFRelease(errorString);
	    SetObjectListFromPropertyList(GetWindowDocStoragePtr(w), propList);
	    CFRelease(propList);
	    HIWindowSetProxyFSRef(w, fsRef);
	}
	else	// pass it to ImageIO. If ImageIO cannot deal with it, imgSrc is NULL.
	{
	    CGImageSourceRef imgSrc = CGImageSourceCreateWithURL(url, NULL);
	    AttachCGImageSourceToWindow(w, imgSrc);
	    if (imgSrc == NULL)
		err = kUnsupportedFileFormat;
	    CFRelease(imgSrc);
	}
	
	CFRelease( info.extension );
    }

    HIViewSetNeedsDisplay(GetWindowDocStoragePtr(w)->theScrollView, true);
    return err;
}


//-------------------------------------------------------
// adapted from DTS/CarbonAppTemplate.
// If we already have a document window, put the first file into it,
// and create new windows for any remaining files.
// If "printThem" is requested, send a print command to each window
// and then a close command.
OSStatus OpenSelectedFiles(AEDescList selection)
{
    extern IBNibRef gOurNibRef;
    long i, count = 0;
    OSStatus status = AECountItems(&selection, &count);
    require_noerr(status, CantGetCount);

    WindowRef w = FrontNonFloatingWindow();

    if (w == NULL)
	w = NewCSkWindow(gOurNibRef, CSkToolPalette(gOurNibRef));
	
    for (i = 1; i <= count; i++)
    {
	FSRef tFSRef;
	
	status = AEGetNthPtr(&selection, i, typeFSRef, NULL, NULL, &tFSRef, sizeof(FSRef), NULL);
	if (status != noErr) continue;
	
	Boolean aliasFileFlag, folderFlag;
	status = FSIsAliasFile(&tFSRef, &aliasFileFlag, &folderFlag);
	if (status != noErr) continue;

	if (!folderFlag)
	{
	    if (i > 1)	// create new windows for each additional file
		w = NewCSkWindow(gOurNibRef, CSkToolPalette(gOurNibRef));
	    status = OpenFileForWindow(w, &tFSRef);
	}
    }
    
CantGetCount:
    return status;
}   // OpenSelectedFiles


//--------------------------------------------------------------------------------------------------
// Make sure "Copy" and "Paste" are correctly enabled/disabled based on the
// pasteboard's avalibility and if their is anything to paste.
static void DoCommandUpdateStatus(EventRef inEvent, WindowRef window, DocStorage* docStP)
{
#pragma unused(inEvent, window, docStP)		// for now
    MenuRef	    menu;
    MenuItemIndex   unused;
    PasteboardRef   pasteBoardRef;
    
    // Get the pasteboard or NULL if if it can't be created.
    pasteBoardRef = GetPasteboard();
    
    // For now, if we have a pasteboard enable "Copy" to put a (potentially empty) pdf on the pasteboard
    GetIndMenuItemWithCommandID(NULL, kHICommandCopy, 1, &menu, &unused);
    if ( pasteBoardRef != NULL)
	EnableMenuCommand(menu, kHICommandCopy);
    else
	DisableMenuCommand(menu, kHICommandCopy);
    
    // For now, enable "Paste" only if we have a pastebord and their is a pdf on the pasteboard
    GetIndMenuItemWithCommandID(NULL, kHICommandPaste, 1, &menu, &unused);
    if ( pasteBoardRef != NULL && PasteboardContainsPDF(pasteBoardRef, NULL) )
	EnableMenuCommand(menu, kHICommandPaste);
    else
	DisableMenuCommand(menu, kHICommandPaste);
}

//--------------------------------------------------------------------------------------------------
// We use the slider positions on the "scale" slider logarithmically: The five evenly spaced tick marks
// should correspond to scaling factors of 0.25, 0.5, 1.0, 2.0, 4.0. With minimum and maximum control
// values of 0 ... 80 (roughly corresponding to display pixels), the formula becomes
// scale = 25 * (2 ^ (value / 20)). A control value of 40 corresponds to scale = 1.0.
static float ScaleCtlValueToScalingFactor(int controlValue)
{
    float scale = 1.0;
    require((controlValue >= 0) && (controlValue <= 80), ControlValueOutOfRange);

    scale = 0.25 * pow(2.0, 0.05 * controlValue);

ControlValueOutOfRange:
    return scale;
}

//--------------------------------------------------------------------------------------------------
// Handle a relatively long list of window HICommands:

static OSStatus DoCommandProcess(EventRef inEvent, WindowRef window, DocStorage* docStP)
{
    OSStatus            err	= eventNotHandledErr;
    CGrgba              color;
    HICommandExtended   command;
    
    GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command );

    switch (command.commandID)
    {
	case kHICommandCopy:
	// for now, only demonstrate how to put the current document content as 'pdf' on the clip board
	{
	    err = AddWindowContentToPasteboardAsPDF( GetPasteboard(), docStP);
	}
	break;

	case kHICommandPaste:
	// demonstrate how to paste in a 'pdf' from the clip board
	{
	    CFDataRef pdfData;
	    if ( PasteboardContainsPDF(GetPasteboard(), &pdfData) )
	    {
		AttachPDFToWindow(window, pdfData); // this will retain the pdfData in docStP
		CFRelease(pdfData);
	    }
	    else
	    {
		fprintf(stderr, "kHICommandPaste: Paste menu item should have been disabled!\n");
	    }
	    err = noErr;
	}
	break;

        case kHICommandSave:
//	    (void)SaveTheDocument(window, docStP);
//	    err = noErr;
	break;
	
        case kHICommandSaveAs:
	    (void)SaveAsCSkDocument(window, docStP);
	    err = noErr;
        break;

        case kHICommandRevert:
//	    (void)RevertToSaved(window, docStP);
//	    err = noErr;
	break;
	
        case kHICommandClose:
            // For now, do nothing here; kEventWindowClose will deal with window closing.
	    // Later, we'll ask whether a "dirty" window needs to be saved.
        break;
		
        case kHICommandPageSetup:
        case kHICommandPrint:
	{
	    ProcessPrintCommand(docStP, command.commandID);
	    err = noErr;
	}
	break;
        
        case kCmdWritePDF:
	    (void)SaveAsPDFDocument(window, docStP);
	    err = noErr;
	    break;
			
        case kHICommandClear:       
	    RemoveSelectedDrawObjs(&docStP->objList); 
	    err = noErr;
	    break;
        
	case kHICommandSelectAll:
	    CSkObjListSetSelectState(&docStP->objList, true);
	    err = noErr;
	    break;
        
        case kCmdDuplicate:
	    DuplicateSelectedDrawObjs(&docStP->objList, docStP->dupOffset.x, docStP->dupOffset.y);
	    err = noErr;
	    break;
        
        case kCmdMoveForward:
	    MoveObjectForward(&docStP->objList);
	    err = noErr;
	    break;

        case kCmdMoveToFront:
	    MoveObjectToFront(&docStP->objList);
	    err = noErr;
	    break;
        
        case kCmdMoveBackward:
	    MoveObjectBackward(&docStP->objList);
	    err = noErr;
	    break;
        
        case kCmdMoveToBack:
	    MoveObjectToBack(&docStP->objList);
	    err = noErr;
	    break;
        
	// Line Width
	case kCmdWidthNone:      
	    SetLineWidthOfSelecteds(&docStP->objList, 0.0);
	    err = noErr;
	    break;
	case kCmdWidthOne:       
	    SetLineWidthOfSelecteds(&docStP->objList, 1.0);
	    err = noErr;
	    break;
	case kCmdWidthTwo:       
	    SetLineWidthOfSelecteds(&docStP->objList, 2.0);
	    err = noErr;
	    break;
	case kCmdWidthFour:      
	    SetLineWidthOfSelecteds(&docStP->objList, 4.0);
	    err = noErr;
	    break;
	case kCmdWidthEight:     
	    SetLineWidthOfSelecteds(&docStP->objList, 8.0);
	    err = noErr;
	    break;
	case kCmdWidthSixteen:   
	    SetLineWidthOfSelecteds(&docStP->objList, 16.0);
	    err = noErr;
	    break;
	case kCmdWidthThinner:   
	    SetLineWidthOfSelecteds(&docStP->objList, kMakeItThinner);
	    err = noErr;
	    break;
	case kCmdWidthThicker:
	    SetLineWidthOfSelecteds(&docStP->objList, kMakeItThicker);
	    err = noErr;
	    break;
	case kCmdLineWidthChanged:  
	    SetLineWidthOfSelecteds(&docStP->objList, CSkToolPaletteGetLineWidth(docStP->toolPalette));
	    err = noErr;
	    break;
			
	// Line Cap
	case kCmdCapButt:        
	    SetLineCapOfSelecteds(&docStP->objList, kCGLineCapButt);
	    err = noErr;
	    break;
	case kCmdCapRound:       
	    SetLineCapOfSelecteds(&docStP->objList, kCGLineCapRound);
	    err = noErr;
	    break;
	case kCmdCapSquare:      
	    SetLineCapOfSelecteds(&docStP->objList, kCGLineCapSquare);
	    err = noErr;
	    break;
	    case kCmdLineCapChanged:
	    SetLineCapOfSelecteds(&docStP->objList, CSkToolPaletteGetLineCap(docStP->toolPalette));
	    err = noErr;
	    break;
		
	// Line Join
	case kCmdJoinMiter:
	    SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinMiter);
	    err = noErr;
	    break;
	case kCmdJoinRound:
	    SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinRound);
	    err = noErr;
	    break;
	case kCmdJoinBevel:
	    SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinBevel);
	    err = noErr;
	    break;
	    case kCmdLineJoinChanged:
	    SetLineJoinOfSelecteds(&docStP->objList, CSkToolPaletteGetLineJoin(docStP->toolPalette));
	    err = noErr;
	    break;
		
	// Line Style
	case kCmdStyleSolid:
	    SetLineStyleOfSelecteds(&docStP->objList, kStyleSolid);
	    err = noErr;
	    break;
	case kCmdStyleDashed:
	    SetLineStyleOfSelecteds(&docStP->objList, kStyleDashed);
	    err = noErr;
	    break;
	    case kCmdLineStyleChanged:
	    SetLineStyleOfSelecteds(&docStP->objList, CSkToolPaletteGetLineStyle(docStP->toolPalette));
	    err = noErr;
	    break;
        
        case kCmdStrokeColorChanged:
            SetStrokeColorOfSelecteds(&docStP->objList, CSkToolPaletteGetStrokeColor(docStP->toolPalette, &color));
	    err = noErr;
	    break;
        
        case kCmdFillColorChanged:
            SetFillColorOfSelecteds(&docStP->objList, CSkToolPaletteGetFillColor(docStP->toolPalette, &color));
			err = noErr;
	    break;
        
        case kCmdStrokeAlphaChanged:
        {
            ControlID   cntlID  = { kControlSignaturePalette, kStrokeAlphaSlider };
            ControlRef  control;
            CGrgba      color;
            float       alpha;
            
            GetControlByID(docStP->toolPalette, &cntlID, &control);
            alpha = (float)GetControlValue(control) / 100.0;
            CSkToolPaletteGetStrokeColor(docStP->toolPalette, &color);
            color.a = alpha;
            CSkToolPaletteSetStrokeColor(docStP->toolPalette, &color);
            SetStrokeAlphaOfSelecteds(&docStP->objList, alpha);
			err = noErr;
        }
        break;
        
        case kCmdFillAlphaChanged:
        {
            ControlID   cntlID  = { kControlSignaturePalette, kFillAlphaSlider };
            ControlRef  control;
            CGrgba      color;
            float       alpha;
            
            GetControlByID(docStP->toolPalette, &cntlID, &control);
            alpha = (float)GetControlValue(control) / 100.0;
            CSkToolPaletteGetFillColor(docStP->toolPalette, &color);
            color.a = alpha;
            CSkToolPaletteSetFillColor(docStP->toolPalette, &color);
            SetFillAlphaOfSelecteds(&docStP->objList, alpha);
			err = noErr;
        }
        break;

        case kCmdScalingValueChanged:
	{
	    ControlID   cntlID  = { kControlSignaturePalette, kScaleSlider };
	    ControlRef  control;
		    
	    GetControlByID(docStP->toolPalette, &cntlID, &control);
	    docStP->scale = ScaleCtlValueToScalingFactor(GetControlValue(control));

	    // Sending an event to the HIScrollView to let it know its scrollable view has changed.
	    // The new value of docStP->scale will be picked up in kEventScrollableGetInfo.
	    EventRef theEvent;
	    CreateEvent(NULL, kEventClassScrollable, kEventScrollableInfoChanged, GetCurrentEventTime(), kEventAttributeUserEvent, &theEvent);
	    SendEventToEventTarget(theEvent, GetControlEventTarget(docStP->theScrollView));
	    ReleaseEvent(theEvent);
	    HIViewSetNeedsDisplay(docStP->theScrollView, true);
	    err = noErr;
	}
	break;
	
	case kCmdNextPageOrImageIndex:
	{
	    if (SetPageNumberOrImageIndex(docStP, docStP->indexOrPageNo + 1))
		HIViewSetNeedsDisplay(docStP->theScrollView, true);
	}
	break;
	
	case kCmdPrevPageorImageIndex:
	{
	    if (SetPageNumberOrImageIndex(docStP, docStP->indexOrPageNo - 1))
		HIViewSetNeedsDisplay(docStP->theScrollView, true);
	}
	
    }   // switch commandID
    
    HIViewRef contentView;
    HIViewFindByID(HIViewGetRoot(window), kHIViewWindowContentID, &contentView);
    HIViewSetNeedsDisplay(contentView, true);

    return err;
}   // DoCommandProcess

//-----------------------------------------------------------------------------------------------------------------------
static UInt32 MapKeyCharToCommand(char c)
{
    UInt32 cmd = 0;
    switch (c)
    {
	case kBackspaceCharCode:    cmd = kHICommandClear;		break;
	case kLeftArrowCharCode:    cmd = kCmdPrevPageorImageIndex;	break;
	case kRightArrowCharCode:   cmd = kCmdNextPageOrImageIndex;	break;
	case kUpArrowCharCode:	    cmd = kCmdFirstPageOrImageIndex;	break;
	case kDownArrowCharCode:    cmd = kCmdLastPageOrImageIndex;	break;
    }
    return cmd;
}

//-----------------------------------------------------------------------------------------------------------------------
static pascal OSStatus CSkWindowEventHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    #pragma unused ( inCallRef )
    OSStatus	err	    = eventNotHandledErr;
    UInt32	eventKind   = GetEventKind( inEvent );
    UInt32	eventClass  = GetEventClass( inEvent );

    WindowRef   window  = (WindowRef)inUserData;
    DocStorage*	docStP = GetWindowDocStoragePtr(window);
    
    switch ( eventClass )
    {
    case kEventClassKeyboard:
        if (eventKind == kEventRawKeyDown )
        {
            char ch;
            
            err = GetEventParameter( inEvent, 
                                    kEventParamKeyMacCharCodes, 
                                    typeChar,
                                    NULL,
                                    sizeof( char ), 
                                    NULL, 
                                    &ch );
            switch (ch)
	    {
		case kBackspaceCharCode: 
		case kLeftArrowCharCode:
		case kRightArrowCharCode:
		case kUpArrowCharCode:
		case kDownArrowCharCode:
		{
		    HICommand hiCmd;
		    memset(&hiCmd, 0, sizeof(hiCmd));
		    hiCmd.commandID = MapKeyCharToCommand(ch);
		    if (hiCmd.commandID != 0)
			SendWindowCommandEvent(window, &hiCmd);
		}
		break;
		
		default:    
		break;
	    }
        }
        
    case kEventClassWindow:
	if ( eventKind == kEventWindowClose )	// Window is about to close
	{
		// check if dirty, and if so, whether to save the document
	}
	else if ( eventKind == kEventWindowClosed )	// Dispose extra window storage here
	{
		ReleaseDocumentStorage( docStP );
		// Should check whether this is the last window, and if so, close the toolPalette, too
	}
        else if ( eventKind == kEventWindowBoundsChanged )
        {
	    UInt32 attributes = 0;
	    GetEventParameter(inEvent, kEventParamAttributes, typeUInt32, NULL, sizeof(UInt32), NULL, &attributes);
	    {
		if (attributes & kWindowBoundsChangeOriginChanged)	// need to move the overlayWindow along!
		{
		    UpdateOverlayWindow(docStP, docStP->theScrollView, &docStP->overlayWindow, &docStP->overlayView);
		}
		
		if (attributes & kWindowBoundsChangeSizeChanged)	// don't care about kWindowBoundsChangeOriginChanged
		{
		    HIViewRef		contentView;
		    HIViewFindByID(HIViewGetRoot(window), kHIViewWindowContentID, &contentView);
		    HIRect			bounds;
		    HIViewGetBounds( contentView, &bounds );
		    HIViewSetFrame( docStP->theScrollView, &bounds );
		    // Also update the overlay window
		    UpdateOverlayWindow(docStP, docStP->theScrollView, &docStP->overlayWindow, &docStP->overlayView);
		    err = noErr;
		}
	    }
	}
        break;
            
    case kEventClassCommand:
        if ( eventKind == kEventCommandUpdateStatus )
	{
	    DoCommandUpdateStatus(inEvent, window, docStP);
	    err = noErr;
	}
        else if ( eventKind == kEventCommandProcess )
        {
            err = DoCommandProcess(inEvent, window, docStP);
        }
        break;
    }

    return err;
}

//------------------------------------------------------------------------------------------------
WindowRef NewCSkWindow(IBNibRef theNibRef, WindowRef toolPalette)
{
    const EventTypeSpec	windowEvents[]	=   {   { kEventClassCommand,   kEventCommandProcess },
						{ kEventClassCommand,   kEventCommandUpdateStatus },
                                                { kEventClassKeyboard, 	kEventRawKeyDown },
                                                { kEventClassWindow,    kEventWindowBoundsChanged },
                                                { kEventClassWindow,    kEventWindowClose },
                                                { kEventClassWindow,    kEventWindowClosed }
                                            };
    WindowRef window;
    OSStatus err = CreateWindowFromNib(theNibRef, CFSTR("HIViewWindow"), &window);
    require(err == noErr, CantCreateWindow);

    InstallWindowEventHandler(window, CSkWindowEventHandler, GetEventTypeCount(windowEvents), windowEvents, window, NULL);

    DocStorage*  docStP = CreateDocumentStorage(window, toolPalette);
    SetWindowProperty(window, kCSkSignature, kCSkPerWindowStorage, sizeof(DocStorage*), &docStP);
    
    // Create a scroll view in the window with our DocumentView as the scrollable canvas view
    HIViewRef contentView;
    OptionBits options = kHIScrollViewOptionsVertScroll | kHIScrollViewOptionsHorizScroll | kHIScrollViewOptionsAllowGrow;

    // Make scroll view
    err = HIScrollViewCreate(options, &docStP->theScrollView);
    require(err == noErr, CantCreateScrollView);

    // Bind it to the window's contentView
    HIRect bounds;
    HIViewFindByID(HIViewGetRoot(window), kHIViewWindowContentID, &contentView);
    HIViewAddSubview(contentView, docStP->theScrollView);
    HIViewGetBounds(contentView, &bounds);
    HIViewSetFrame(docStP->theScrollView, &bounds);
    HIViewSetVisible(docStP->theScrollView, true);

    // and, while we are at it, create the scrollable DocumentView behind the scrollView.
    // Note that the "page" is offset by kTopMargin/kLeftMargin, and that we keep the same margins at
    // the bottom and right edges of the "page".
    const HIViewID viewID = { kDocumentViewSignature, 0 };
    const Rect docViewBounds = { 0, 0, kDefaultDocHeight + 2 * kTopMargin, kDefaultDocWidth + 2 * kLeftMargin };
    err = CSkDocumentViewCreate(docStP->theScrollView, &docViewBounds, &viewID);
    require(err == noErr, CantCreateDocumentView);

    // If we already have a document window, reposition the new one
    WindowRef currentDocWindow = FrontNonFloatingWindow();
    if (currentDocWindow != NULL)
	RepositionWindow(window, currentDocWindow, kWindowCascadeOnParentWindow);
    SetUserFocusWindow(window);

    // We also create the overlayWindow for the scrollView, at this point.
    // This needs to be updated each time the window moves or gets resized!
    docStP->overlayWindow = NULL;
    err = UpdateOverlayWindow(docStP, docStP->theScrollView, &docStP->overlayWindow, &docStP->overlayView);
    require(err == noErr, CantCreateOverlayWindow);

    ShowWindow(window);

    return window;
    
CantCreateOverlayWindow:
CantCreateDocumentView:
CantCreateScrollView:
    ReleaseDocumentStorage(docStP);
    DisposeWindow(window);
CantCreateWindow:
    return NULL;
}
