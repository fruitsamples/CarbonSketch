/*
    File:       NavServicesHandling.c
    
    Contains:	Code to handle the save dialog for our sample application.

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

    Copyright © 1999-2005 Apple Computer, Inc., All Rights Reserved
*/


#include "NavServicesHandling.h"
#include "CSkDocStorage.h"
#include "CSkWindow.h"

#define	kFileCreatorPDF			'prvw'
#define kFileTypePDF			'PDF '

#define	kFileCreatorCSk			'CSk '
#define kFileTypeCSk			'CSk '

#define kFileTypePDFCFStr		CFSTR("%@.pdf")		// our format string for making the save as file name
#define kFileTypeCSkCFStr		CFSTR("%@.csk")


struct OurNavDialogData {
    bool	    isOpenDialog;
    bool	    userCanceled;
    bool	    savePDF;
    NavDialogRef    dialogRef;
    void*	    userDataP;
};
typedef struct OurNavDialogData OurNavDialogData;


//-----------------------------------------------------------------------------------------------------------------------
static OSStatus MakePDFDocument(DocStoragePtr docStP, CFURLRef url)	
{
    OSStatus                err         = -1;   // generic error code: watch console output!
    CFMutableDictionaryRef  dict        = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                                                    &kCFTypeDictionaryKeyCallBacks, 
                                                                    &kCFTypeDictionaryValueCallBacks); 
    if (dict != NULL) 
    {
        CGContextRef    ctx         = CGPDFContextCreateWithURL(url, &docStP->pageRect, dict);
        CFStringRef     stringRef;    // Add some producer information to our PDF file
        
        CopyWindowTitleAsCFString(docStP->ownerWindow, &stringRef);
	CFDictionaryAddValue(dict, CFSTR("Title"), stringRef);
	CFRelease(stringRef);
	CFDictionaryAddValue(dict, CFSTR("Creator"), CFSTR("CarbonSketch"));

        if (ctx != NULL)
        {
	    CGContextBeginPage(ctx, &docStP->pageRect);
	    docStP->shouldDrawGrid = false;
	    DrawThePage(ctx, docStP);
	    docStP->shouldDrawGrid = true;
	    CGContextEndPage(ctx);
	    CGContextRelease(ctx);
            err = noErr;
        }
	CFRelease(dict);
    }
    else 
    {
        fprintf(stderr, "CFDictionaryCreateMutable FAILED\n");
    }
    
    return err;
}   // MakePDFDocument


//-----------------------------------------------------------------------------------------------------------------------
static OSStatus MakeCSkDocument(DocStoragePtr docStP, CFURLRef url)	
{
    OSStatus err = noErr;   // generic error code: watch console output!

    CFWriteStreamRef stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, url); 
    (void)CFWriteStreamOpen(stream);

    CFStreamStatus status = CFWriteStreamGetStatus(stream);
    if (status != kCFStreamStatusOpen)
    {
	fprintf(stderr, "CFWriteStreamOpen: status = %d\n", (int)status);
	return -1;
    }
    
    CFPropertyListRef docPList = CSkCreatePropertyList(docStP);

    if (CFWriteStreamCanAcceptBytes(stream))
    {
	CFStringRef errorString = NULL;  
	CFPropertyListWriteToStream(docPList, stream, kCFPropertyListXMLFormat_v1_0, &errorString);  
	if (errorString != NULL) 
	{  
	    fprintf(stderr, "CFPropertyListWriteToStream failed\n");
	    err = -2;  // watch console output!
	}
    }
    else
    {
	fprintf(stderr, "CFWriteStreamCanAcceptBytes() returned FALSE\n");
    }
    
    CFWriteStreamClose(stream);  
    CFRelease(stream);      
    
    CFRelease(docPList);
    
    return err;
}   // MakeCSkDocument


//-----------------------------------------------------------------------------------------------------------------------
static OSStatus DoFSRefSave(const OurNavDialogData* dialogDataP, NavReplyRecord* reply, AEDesc* actualDescP)
{
    OSStatus 	err;
    FSRef 	fileRefParent;
	    
    err = AEGetDescData( actualDescP, &fileRefParent, sizeof(FSRef) );
    if (err != noErr )
	return err;

    // get the name data and its length:	
    HFSUniStr255    nameBuffer;
    UniCharCount    sourceLength = (UniCharCount)CFStringGetLength( reply->saveFileName );

    CFStringGetCharacters(reply->saveFileName, CFRangeMake(0, sourceLength), (UniChar*)&nameBuffer.unicode);
    
    if (sourceLength == 0)
	return -1;
	
    if (reply->replacing)
    {
	// delete the file we are replacing:
	FSRef fileToDelete;
	err = FSMakeFSRefUnicode( &fileRefParent, sourceLength, nameBuffer.unicode, kTextEncodingUnicodeDefault, &fileToDelete );
	if (err == noErr )
	{
	    err = FSDeleteObject( &fileToDelete );
	    if ( err == fBsyErr )
	    {
		fprintf(stderr, "FSDeleteObject returned \"File Busy\" Error %d\n", fBsyErr);
	    }
	}
    }
		    
    if ( err != noErr )
	return err;
    
    FSRef newFSRef;
    FSCatalogInfo catalogInfo;
    FInfo fileInfo;
    
    memset(&fileInfo, 0, sizeof(FInfo));
    
    fileInfo.fdType = (dialogDataP->savePDF ? kFileTypePDF : kFileTypeCSk);
    fileInfo.fdCreator = (dialogDataP->savePDF ? kFileCreatorPDF : kFileCreatorCSk);
    
    memcpy(&catalogInfo.finderInfo, &fileInfo, sizeof(FInfo));

    err = FSCreateFileUnicode( &fileRefParent, 
				sourceLength, 
				nameBuffer.unicode,
				kFSCatInfoFinderInfo,
				&catalogInfo,
				&newFSRef,	
				NULL);                
    if (err != noErr)
	return err;

    CFURLRef saveURL = CFURLCreateFromFSRef(NULL, &newFSRef);


    if (saveURL == NULL)
    {
	fprintf(stderr, "CFURLCreateFromFSRef FAILED\n");
	return -1;
    }

    if (dialogDataP->savePDF )
    {
	// delete the file we just made for making the FSRef
	FSDeleteObject(&newFSRef);
	err = MakePDFDocument((DocStoragePtr)dialogDataP->userDataP, saveURL);
    }
    else
    {
	FSIORefNum forkRefNum;
	HFSUniStr255 dataForkName;
	FSGetDataForkName(&dataForkName);
	err = FSOpenFork(&newFSRef, dataForkName.length, dataForkName.unicode, fsRdWrPerm, &forkRefNum);
	err = MakeCSkDocument((DocStoragePtr)dialogDataP->userDataP, saveURL);
    }
    
    CFRelease(saveURL);
    
    if (err == noErr)
    {
	err = NavCompleteSave( reply, kNavTranslateInPlace );
	if (err)
	    fprintf(stderr, "NavCompleteSave returned error %d\n", (int)err);
    }
    
    if (err) // delete the file if an error occurred
    {
	FSDeleteObject(&newFSRef);
    }

    return err;
}


//-----------------------------------------------------------------------------------------------------------------------
static pascal void NavEventProc( NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms, void* callBackUD )
{
    OurNavDialogData *dialogDataP = (OurNavDialogData*)callBackUD;
    OSStatus 	err = noErr;		        
	
    switch ( callBackSelector )
    {
	case kNavCBUserAction:
	{
	    NavReplyRecord 	reply;
	    NavUserAction 	userAction = 0;
	    
	    err = NavDialogGetReply( callBackParms->context, &reply );
	    if (err == noErr )
	    {
		OSStatus tempErr;
		userAction = NavDialogGetUserAction( callBackParms->context );
				
		switch ( userAction )
		{
		    case kNavUserActionOpen:
		    {        
			if (dialogDataP != NULL )
			{	
			    OpenSelectedFiles(reply.selection);
			}
		    }
		    break;
				
		    case kNavUserActionSaveAs:
		    {
			if ( dialogDataP != NULL )
                        {
			    AEDesc actualDesc;
                            err = AECoerceDesc( &reply.selection, typeFSRef, &actualDesc );
			    if ( err == noErr )
			    {	
				// the coercion succeeded as an FSRef, so use HFS+ APIs to save the file:
				err = DoFSRefSave( dialogDataP, &reply, &actualDesc);
				AEDisposeDesc( &actualDesc );
			    }
			}
			break;
		    }
				    				    
		    case kNavUserActionCancel:
                        break;
					    
		    case kNavUserActionNewFolder:
                        break;
		}
	          
		tempErr = NavDisposeReply( &reply );
		if (!err)
		    err = tempErr;
            }
            break;
	}
			
	case kNavCBTerminate:
	{
//	    fprintf(stderr, "kNavCBTerminate\n");
	    if ( dialogDataP != NULL )
	    {
		if (dialogDataP->dialogRef)
		{
//		    fprintf(stderr, "  kNavCBTerminate: calling NavDialogDispose\n");
		    NavDialogDispose(dialogDataP->dialogRef );
		}
		dialogDataP->dialogRef = NULL;
		free(dialogDataP);
	    }
	}
	break;
    }
}

//-----------------------------------------------------------------------------------------------------------------------
// this code originates from the NavServices sample code in the CarbonLib SDK
OSStatus SaveAsPDFDocument (WindowRef w, void* ourDataP)
{
    OSStatus 			err = noErr;
    static NavEventUPP          gNavEventProc = NULL;		// event proc for our Nav Dialogs 
    NavDialogCreationOptions	dialogOptions;

    if (gNavEventProc == NULL)
    {
        gNavEventProc = NewNavEventUPP(NavEventProc);
        if (!gNavEventProc)
            return memFullErr;
    }

    err = NavGetDefaultDialogCreationOptions( &dialogOptions );
    if (err == noErr )
    {
	OurNavDialogData*  dialogDataP = NULL;
	CFStringRef         tempString;
        
	CopyWindowTitleAsCFString(w, &tempString);
        dialogOptions.saveFileName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, kFileTypePDFCFStr, tempString);
	CFRelease(tempString);
	
	// make the dialog modal to our parent doc, AKA sheets
        dialogOptions.parentWindow = w;
	dialogOptions.modality = kWindowModalityWindowModal;
	
	dialogDataP = (OurNavDialogData*)malloc(sizeof(OurNavDialogData));
	if (dialogDataP)
        {
	    dialogDataP->dialogRef  = NULL;
	    dialogDataP->userDataP  = ourDataP;
            dialogDataP->savePDF = true;
	    
	    err = NavCreatePutFileDialog(&dialogOptions, kFileTypePDF, kFileCreatorPDF,
						    gNavEventProc, dialogDataP,
						    &dialogDataP->dialogRef);
	    if ((err == noErr) && (dialogDataP->dialogRef != NULL))
	    {
		err = NavDialogRun( dialogDataP->dialogRef );
		if (err != noErr)
		{
                    fprintf(stderr, "NavDialogRun returned error %d\n", (int)err);
		}
	    }
            else
            {
                fprintf(stderr, "NavCreatePutFileDialog returned error %d\n", (int)err);
            }
	}
        else
	    err = memFullErr;
       	
     	if ( dialogOptions.saveFileName != NULL )
	    CFRelease( dialogOptions.saveFileName );
    }
    return err;
}   // SaveAsPDFDocument


//-----------------------------------------------------------------------------------------------------------------------
OSStatus SaveAsCSkDocument (WindowRef w, void* ourDataP)
{
    OSStatus 			err = noErr;
    static NavEventUPP          gNavEventProc = NULL;		// event proc for our Nav Dialogs 
    NavDialogCreationOptions	dialogOptions;

    if (gNavEventProc == NULL)
    {
        gNavEventProc = NewNavEventUPP(NavEventProc);
        if (!gNavEventProc)
            return memFullErr;
    }

    err = NavGetDefaultDialogCreationOptions( &dialogOptions );
    if (err == noErr )
    {
	OurNavDialogData*  dialogDataP = NULL;
	CFStringRef         tempString;
        
	CopyWindowTitleAsCFString(w, &tempString);
        dialogOptions.saveFileName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, kFileTypeCSkCFStr, tempString);
	CFRelease(tempString);
	
	// make the dialog modal to our parent doc, AKA sheets
        dialogOptions.parentWindow = w;
	dialogOptions.modality = kWindowModalityWindowModal;
	
	dialogDataP = (OurNavDialogData*)malloc(sizeof(OurNavDialogData));
	if (dialogDataP)
        {
	    dialogDataP->dialogRef  = NULL;
	    dialogDataP->userDataP  = ourDataP;
            dialogDataP->savePDF = false;

	    err = NavCreatePutFileDialog(&dialogOptions, kFileTypeCSk, kFileCreatorCSk,
						    gNavEventProc, dialogDataP,
						    &dialogDataP->dialogRef);
	    if ((err == noErr) && (dialogDataP->dialogRef != NULL))
	    {
		err = NavDialogRun( dialogDataP->dialogRef );
		if (err != noErr)
		{
                    fprintf(stderr, "NavDialogRun returned error %d\n", (int)err);
		}
	    }
            else
            {
                fprintf(stderr, "NavCreatePutFileDialog returned error %d\n", (int)err);
            }
	}
        else
	    err = memFullErr;
       	
     	if ( dialogOptions.saveFileName != NULL )
	    CFRelease( dialogOptions.saveFileName );
    }
    return err;
}   // SaveAsCSkDocument


//-------------------------------------------------------
static pascal Boolean MyNavFilter(AEDesc *theItem, void *info, void *callBackUD, NavFilterModes filterMode)
{
#pragma unused(info, callBackUD, filterMode)
//    extern FSRef gApplicationBundleFSRef;
    Boolean canViewItem = false;
    
    if (theItem->descriptorType == typeFSRef)
    {
	FSRef fsRef;
	LSItemInfoRecord infoRec;
	OSStatus err = AEGetDescData(theItem, &fsRef, sizeof(fsRef));
	require_noerr(err, CantGetFSRef);
	
	// Ask LaunchServices for information about the item
	err = LSCopyItemInfoForRef(&fsRef, kLSRequestAllInfo, &infoRec);
	require((err == noErr) || (err == kLSApplicationNotFoundErr), LaunchServicesError);
	
	if ((infoRec.flags & kLSItemInfoIsContainer) != 0)
	{
	    canViewItem	= true;
	}
	else
	{
//	    err = LSCanRefAcceptItem(&fsRef, &gApplicationBundleFSRef, kLSRolesViewer, kLSAcceptDefault, &canViewItem);		
	    canViewItem = true;	//@@@ experimental only
	}
    }
    
LaunchServicesError:
CantGetFSRef:
    return canViewItem;
}   // MyNavFilter


//-------------------------------------------------------
// Bring up a ChooseFileDialog and allow .pdf or .csk files to be opened (as specified in the CarbonSketch Info.plist).
// If more than one file is selected, put the first into the front window (if there is one), and create new 
// windows for the remaining ones (this happens inside OpenSelectedFiles).

OSStatus OpenAFile( void )
{
    OSStatus err = noErr;

    NavDialogCreationOptions navOptions;
    err = NavGetDefaultDialogCreationOptions(&navOptions);
    require_noerr(err, CantGetDefaultOptions);
    
    navOptions.preferenceKey = 1;
    
    NavDialogRef theDialog = NULL;
    err = NavCreateChooseFileDialog(&navOptions, NULL, NULL, NULL, MyNavFilter, NULL, &theDialog);
    require_noerr(err, CantCreateDialog);
    
    err = NavDialogRun(theDialog);
    require_noerr(err, CantRunDialog);
    
    NavReplyRecord aNavReplyRecord;
    err = NavDialogGetReply(theDialog, &aNavReplyRecord);
    require((err == noErr) || (err == userCanceledErr), CantGetReply);
    
    NavDialogDispose(theDialog);
    theDialog = NULL;
    
    if (aNavReplyRecord.validRecord)
	err = OpenSelectedFiles(aNavReplyRecord.selection);
    else
	err = userCanceledErr;
    
    NavDisposeReply(&aNavReplyRecord);
    
CantGetReply:
CantRunDialog:
    if (theDialog != NULL)
	NavDialogDispose(theDialog);
	
CantCreateDialog:
CantGetDefaultOptions:
    return err;
}

