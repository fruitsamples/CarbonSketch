/*
    File:       CSkDocStorage.h
        
    Contains:	Document storage structure (to be attached to window).
                ToDo: Should make it opaque, and move some code from CSkWindow.c into
                a new file CSkDocStorage.c.

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

    Copyright © 2005 Apple Computer, Inc., All Rights Reserved
*/


#ifndef __CSKDOCSTORAGE__
#define __CSKDOCSTORAGE__

#include <Carbon/Carbon.h>

#ifndef __CSKCONSTANTS__
#include "CSkConstants.h"
#endif

#ifndef __CSKOBJECTS__
#include "CSkObjects.h"
#endif

struct DocStorage	
{
    WindowRef           ownerWindow;        // back reference to owning window
    WindowRef           toolPalette;        // holds current tool selection and drawing attributes
	WindowRef			overlayWindow;
	HIViewRef			overlayView;
    CGContextRef        bmCtx;              // 1x1 bitmap context for hit testing
	HIViewRef			theScrollView;
	HIViewRef			sketchView;			// the sketch view inside the scroll view
    CGAffineTransform   displayCTM;         // apply to windowContext before drawing document content into it (scales + offsets)
    DrawObjList         objList;            // our drawing objects
    CGRect				pageRect;
    CGPoint				pageTopLeft;        // because our "page" is being drawn offset on the background
    CGPoint             dupOffset;          // offset when duplicating selected objects
    float				gridWidth;          // unscaled
    float               scale;              // passed to CGContextScaleCTM
    PMPageFormat		pageFormat;
    PMPrintSettings		printSettings;
    CFDataRef           flattenedPageFormat;
	CFDataRef			pdfData;			// in case we pasted in a pdf from the pasteboard
	CGImageSourceRef	cgImgSrc;			// background image
	CGPDFDocumentRef	pdfDocument;		// temporary storage while waiting for PW-protected PDF to be unlocked
	size_t				indexOrPageNo;		// index of image in cgImgSrc, or page number in PDF
	Boolean				pdfIsProtected;		// remember whether the pdf in window background is PW-protected
	Boolean				pdfAskForPassword;	// if so, draw the window content as "password entry"
	Boolean				pdfIsUnlocked;		// for PW-protected PDFs, after providing the correct PW
	Boolean				shouldDrawGrabbers;	// whether or not the "grabbers" on selected objects should be drawn
	Boolean				shouldDrawGrid;		// whether or not the background grid should be drawn
};
typedef struct DocStorage DocStorage, *DocStoragePtr;

DocStorage* CreateDocumentStorage(WindowRef window, WindowRef toolPalette);
DocStorage* GetWindowDocStoragePtr(WindowRef window);

void	ReleaseDocumentStorage(DocStorage* docStP);

// Assuming a CGContextRef is set up correctly, the above DocStorage is all that's needed to draw the document page.
void DrawThePage(CGContextRef ctx, const DocStorage* docStP);

Boolean SetPageNumberOrImageIndex(DocStorage* docStP, size_t pageNumberOrImageIndex);

// Background is either a grid page, or a CGImage from ImageIO, or a PDF content
void DrawDocumentBackgroundGrid(CGContextRef ctx, CGSize docSize, float gridWidth);
void DrawPDFData(CGContextRef ctx, CGPDFDocumentRef document, size_t pageNo, CGRect destRect);

CFPropertyListRef CSkCreatePropertyList(DocStoragePtr docStP);
void SetObjectListFromPropertyList(DocStoragePtr docStP, CFPropertyListRef propList);

#endif
