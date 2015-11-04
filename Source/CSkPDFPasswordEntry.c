/*
 *  CSkPDFPasswordEntry.c
 *  CarbonSketch
 *
 *  Created by Joseph Maurer on 5/24/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include "CSkPDFPasswordEntry.h"
#include "CSkConstants.h"
#include "CSkUtils.h"

const HIViewID	editTextID = { kCSkSignature, 1 };

struct PWDialogData {
    WindowRef		dialog;
    CGPDFDocumentRef	document;
    Boolean		unlocked;
};
typedef struct PWDialogData PWDialogData;

static OSStatus DoCommandProcess(EventRef inEvent, PWDialogData* dlgData)
{
    OSStatus		status = eventNotHandledErr;	// the return value
    OSStatus		err;	// local error handling
    HICommandExtended   command;
    ControlRef		editTextCtl = NULL;
    char		password[32];		    // 32 is the maximum length of a PDF password
    Size		pwLength = 0;

    GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command );
    err = HIViewFindByID(HIViewGetRoot(dlgData->dialog), editTextID, &editTextCtl);
    require(err == noErr, HIViewFindByID_Failed);
    
    switch (command.commandID)
    {
	case kHICommandOK:
	    err = GetControlData(editTextCtl, 0, kControlEditTextTextTag, 32, password, &pwLength);
	    require(err == noErr, GetControlDataFailed);
	    
	    dlgData->unlocked = CGPDFDocumentUnlockWithPassword(dlgData->document, password);
	    if (dlgData->unlocked)
	    {
		QuitAppModalLoopForWindow(dlgData->dialog);
		SendWindowCloseEvent(dlgData->dialog);
		status = noErr;
	    }
	    else
	    {
		ControlRef staticText;
		const HIViewID id = { kCSkSignature, 2 };
		err = HIViewFindByID(HIViewGetRoot(dlgData->dialog), id, &staticText );
		require(err == noErr, HIViewFindByID_Failed);
		HIViewSetVisible(staticText, true);
		SetControlData(editTextCtl, 0, kControlEditTextTextTag, 0, NULL);
		status = noErr;
	    }
	break;
	
	case kHICommandCancel:
	    QuitAppModalLoopForWindow(dlgData->dialog);
	    SendWindowCloseEvent(dlgData->dialog);
	    status = noErr;
	break;
    }
    return status;

GetControlDataFailed:
HIViewFindByID_Failed:
    return eventNotHandledErr;
}

//-----------------------------------------------------------------------------------------------------------------------
static pascal OSStatus PWDialogEventHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    #pragma unused ( inCallRef )
    OSStatus	err	    = eventNotHandledErr;
    UInt32	eventKind   = GetEventKind( inEvent );
    PWDialogData* dlgData   = (PWDialogData*)inUserData;
     
    switch ( GetEventClass(inEvent) )
    {
	case kEventClassKeyboard:
        if (eventKind == kEventRawKeyDown )
        {
            char ch;
	    HICommand hiCmd;
	    memset(&hiCmd, 0, sizeof(hiCmd));
            
            err = GetEventParameter( inEvent, 
                                    kEventParamKeyMacCharCodes, 
                                    typeChar,
                                    NULL,
                                    sizeof( char ), 
                                    NULL, 
                                    &ch );
            if ((ch == kReturnCharCode) || (ch == kEnterCharCode))
            {
                hiCmd.commandID = kHICommandOK;
		SendWindowCommandEvent(dlgData->dialog, &hiCmd);
		err = noErr;
            }
	    else if (ch == kEscapeCharCode)
	    {
		hiCmd.commandID = kHICommandCancel;
		SendWindowCommandEvent(dlgData->dialog, &hiCmd);
		err = noErr;
	    }
	    else
		err = CallNextEventHandler(inCallRef, inEvent);
        }
	break;
        
	case kEventClassCommand:
        if ( eventKind == kEventCommandUpdateStatus )
	{
//	    DoCommandUpdateStatus(inEvent, window, docStP);
//	    err = noErr;
	}
        else if ( eventKind == kEventCommandProcess )
        {
            err = DoCommandProcess(inEvent, dlgData);
        }
        break;
    }

    return err;
}   // PWDialogEventHandler



//------------------------------------------------------------------------------------------------
static void ShowPDFUnlockDialog(IBNibRef theNibRef, PWDialogData* dlgData)
{	
    const EventTypeSpec	dialogEvents[]	=   {   { kEventClassCommand,   kEventCommandProcess } /*,
                                                { kEventClassKeyboard, 	kEventRawKeyDown } */
                                             };
    OSStatus err = CreateWindowFromNib(theNibRef, CFSTR("PWDialog"), &dlgData->dialog);
    
    if (err == noErr)
    {
	ControlRef editTextCtl = NULL;
	InstallWindowEventHandler(dlgData->dialog, PWDialogEventHandler, GetEventTypeCount(dialogEvents), dialogEvents, dlgData, NULL);
	ShowWindow(dlgData->dialog);
	HIViewFindByID(HIViewGetRoot(dlgData->dialog), editTextID, &editTextCtl);
	SetKeyboardFocus(dlgData->dialog, editTextCtl, kControlFocusNextPart);
	RunAppModalLoopForWindow(dlgData->dialog);
    }
}

//------------------------------------------------------------------------------------------------
Boolean UnlockPDFDocument(CGPDFDocumentRef document)
{
extern IBNibRef gOurNibRef;
    PWDialogData  dlgData;
    
    dlgData.dialog  = NULL;
    dlgData.document = document;
    dlgData.unlocked = false;
    
    ShowPDFUnlockDialog(gOurNibRef, &dlgData);
    
    return dlgData.unlocked;
}
