/*
    File:       CSkDocStorage.c
        
    Contains:	Document storage structure management and document drawing

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

#include "CSkDocStorage.h"

//------------------------------------------------------------------------------------------------------------------
// For convenience, we create this "single pixel CGBitmapContext" for hit-testing right away at creation of a new
// document. Given the purpose of CarbonSketch, there is not much use of it without doing hit-testing, sooner or later.
// Note that we use the genericColorSpace for consistency, even though it doesn't make much technical sense, here.
static CGContextRef CreateSinglePixelBitmapContext()
{
    const int	    width       = 1;
    const int	    height      = 1;
    const int	    rowBytes	= 4 * width;
    const int	    size        = rowBytes * height;
    void*           data        = calloc(size, 1);
    CGColorSpaceRef genericColorSpace  = GetGenericRGBColorSpace();
    CGContextRef    bmCtx       = CGBitmapContextCreate(data, width, height, 8, rowBytes, genericColorSpace, kCGImageAlphaPremultipliedFirst);

    CGContextSetFillColorSpace(bmCtx, genericColorSpace); 
    CGContextSetStrokeColorSpace(bmCtx, genericColorSpace); 

    // After that, all colors passed to CGContextSetFillColor/CGContextSetStrokeColor are arrays
    // float[4] = { r, g, b, a }. In this app, we use the CGrgba type instead, for clarity.

    return bmCtx;
}

//--------------------------------------------------------------------------------------
DocStoragePtr GetWindowDocStoragePtr(WindowRef w)
{
    DocStorage*	docStP;
    GetWindowProperty(w, kCSkSignature, kCSkPerWindowStorage, sizeof(DocStorage*), NULL, &docStP);
    return docStP;
}

//--------------------------------------------------------------------------------------
// Allocation and initialization of document storage.
// Note that we rely on NewPtrClear setting everything else to NULL.

DocStorage* CreateDocumentStorage(WindowRef window, WindowRef toolPalette)
{
    DocStorage*  docStP = (DocStorage*) NewPtrClear( sizeof(DocStorage) );
    // 
    docStP->ownerWindow		= window;
    docStP->toolPalette		= toolPalette;
//  docStP->overlayWindow	= NULL;
//  docStP->overlayContext	= NULL;
//  docStP->theScrollView	= NULL;
//  docStP->sketchView		= NULL;
    docStP->bmCtx		= CreateSinglePixelBitmapContext();
    docStP->pageTopLeft		= CGPointMake(kLeftMargin, kTopMargin);
    docStP->pageRect		= CGRectMake(0, 0, kDefaultDocWidth, kDefaultDocHeight);
    docStP->gridWidth		= kGridWidth;
    docStP->scale		= 1.0;
    docStP->dupOffset		= CGPointMake(9.0, 9.0);
    docStP->pageFormat		= kPMNoPageFormat;
    docStP->printSettings	= kPMNoPrintSettings;
//  docStP->flattenedPageFormat	= NULL;
//  docStP->pdfData		= NULL;
    docStP->pdfIsProtected	= false;    // by default
    docStP->pdfIsUnlocked	= true;	    // else present password entry window
    docStP->shouldDrawGrabbers	= false;    // by default
    docStP->shouldDrawGrid	= true;	    // by default
    return docStP;
}

//--------------------------------------------------------------------------------------
// Make all the necessary "..Release" calls, and deallocate any nested storage
void ReleaseDocumentStorage(DocStorage* docStP)
{
    ReleaseDrawObjList(&docStP->objList);
    
    if (docStP->bmCtx != NULL)
        CGContextRelease(docStP->bmCtx);
     // Don't dispose of toolPalette!
     if (docStP->pdfData != NULL)
	CFRelease(docStP->pdfData);
    if (docStP->pdfDocument != NULL)
	CGPDFDocumentRelease(docStP->pdfDocument);
    if (docStP->cgImgSrc != NULL)
	CFRelease(docStP->cgImgSrc);
    if (docStP->pageFormat != kPMNoPageFormat)
        (void)PMRelease(docStP->pageFormat);
    if (docStP->printSettings != kPMNoPrintSettings)
        (void)PMRelease(docStP->printSettings);
    if (docStP->flattenedPageFormat != NULL)
        CFRelease(docStP->flattenedPageFormat);
}

//--------------------------------------------------------------------------------------
Boolean SetPageNumberOrImageIndex(DocStorage* docStP, size_t pageNumberOrImageIndex)
{
    Boolean didChange = false;
    size_t pageCount = 0;
    if (docStP->pdfDocument != NULL)
	pageCount = CGPDFDocumentGetNumberOfPages(docStP->pdfDocument);
    else if (docStP->cgImgSrc != NULL)
	pageCount = CGImageSourceGetCount(docStP->cgImgSrc);
    if (pageNumberOrImageIndex <= pageCount)   // pageNumberOrImageIndex is 1-based
    {
	docStP->indexOrPageNo = pageNumberOrImageIndex;
	didChange = true;
    }
    return didChange;
}

//--------------------------------------------------------------------------------------
// Draw page 1 of pdfData into the context. Used in "DrawThePage" because we allow PDF data as background.
void DrawPDFData(CGContextRef ctx, CGPDFDocumentRef document, size_t pageNo, CGRect destRect)
{
    CGPDFPageRef	page = CGPDFDocumentGetPage(document, pageNo);
    CGAffineTransform	pageTransform = CGPDFPageGetDrawingTransform(page, kCGPDFArtBox, destRect, 0, true);

    CGContextSaveGState (ctx);
    CGContextConcatCTM (ctx, pageTransform);
    CGContextClipToRect (ctx, destRect);
    CGContextDrawPDFPage(ctx, page);				// Just what it says it does ...
    CGContextRestoreGState (ctx);
}

//--------------------------------------------------------------------------------------
void DrawDocumentBackgroundGrid(CGContextRef ctx, CGSize docSize, float gridWidth)
// Draw document background grid
{
    const CGrgba gridColor = { 0.8, 0.9, 0.8, 1.0 };	// used as float[4] in CGContextSetStrokeColor
    float t;
    
    CGContextSetStrokeColor(ctx, (CGFloat*)&gridColor);
    CGContextSetLineWidth(ctx, 0.5);
    
    t = 0.5;
    while (t < docSize.width)
    {
	CGContextMoveToPoint(ctx, t, 0);
	CGContextAddLineToPoint(ctx, t, docSize.height);
	t += gridWidth;
    }
    t = 0.5;
    while (t < docSize.height)
    {
	CGContextMoveToPoint(ctx, 0.5, t);
	CGContextAddLineToPoint(ctx, docSize.width, t);
	t += gridWidth;
    }

    CGContextMoveToPoint(ctx, docSize.width, 0);
    CGContextAddLineToPoint(ctx, docSize.width, docSize.height);
    CGContextAddLineToPoint(ctx, 0, docSize.height);
    
    CGContextStrokePath(ctx);
}


//--------------------------------------------------------------------------------------------------
// We reuse this routine in NavServicesHandling.c, from "MakePDFDocument":

void DrawThePage(CGContextRef ctx, const DocStorage* docStP)
{
    CGColorSpaceRef genericColorSpace = GetGenericRGBColorSpace();

    // ensure that we are drawing in the correct color space, a calibrated color space
    CGContextSetFillColorSpace(ctx, genericColorSpace); 
    CGContextSetStrokeColorSpace(ctx, genericColorSpace); 
    
    if (docStP->shouldDrawGrid)
	DrawDocumentBackgroundGrid(ctx, docStP->pageRect.size, docStP->gridWidth);
    
    // If we have a background pdf or image, draw it
    if ((docStP->pdfData != NULL) && (docStP->pdfIsUnlocked))
    {
	DrawPDFData(ctx, docStP->pdfDocument, docStP->indexOrPageNo, docStP->pageRect);
    }
    else if (docStP->cgImgSrc != NULL)
    {
	CGImageRef img = CGImageSourceCreateImageAtIndex(docStP->cgImgSrc, docStP->indexOrPageNo - 1, NULL);
	if (img != NULL)
	{
	    size_t width = CGImageGetWidth(img);
	    size_t height = CGImageGetHeight(img);
	    CGRect dstR = CGRectMake(0, 0, width, height);
	    // Move it to the topleft corner of docStP->pageRect
	    dstR = CGRectOffset(dstR, 0, docStP->pageRect.size.height - height);
	    CGContextDrawImage(ctx, dstR, img);
	    CFRelease(img);
	}
	else
	{
	    fprintf(stderr, "CGImageSourceCreateImageAtIndex %d failed\n", (int)docStP->indexOrPageNo);
	}
    }
	
    RenderDrawObjList(ctx, &docStP->objList, docStP->shouldDrawGrabbers);
}

//--------------------------------------------------------------------------------------------------
CFPropertyListRef CSkCreatePropertyList(DocStoragePtr docStP)
{
    CFMutableDictionaryRef docDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    // leave room for additional document features, like PDF encryption, document size etc.
    
    CFMutableArrayRef objArray = CSkObjectListConvertToCFArray(docStP->objList.firstItem);
    CFDictionaryAddValue(docDict, kKeyObjectArray, objArray);
    CFRelease(objArray);
    return docDict;
}

void SetObjectListFromPropertyList(DocStoragePtr docStP, CFPropertyListRef propList)
{
    CFArrayRef objArray = CFDictionaryGetValue(propList, kKeyObjectArray);
    CSkConvertCFArrayToDrawObjectList(objArray, &docStP->objList);
}


