/*********************************************************************
 *----------------------------- DRAG.C ------------------------------*
 *                                                                   *
 * MODULE NAME :  drag.c                 AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  12-05-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It contains the functions     *
 *  necessary to implement container drag/drop.                      *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  MRESULT DragMessage( HWND hwndClient, ULONG msg, MPARAM mp1,     *
 *                       MPARAM mp2 );                               *
 *  VOID    DragInit( HWND hwndClient, PCNRDRAGINIT pcdi );          *
 *  MRESULT DragOver( HWND hwndClient, PCNRDRAGINFO pcdi );          *
 *  VOID    DragDrop( HWND hwndClient, PCNRDRAGINFO pcdi );          *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  12-05-92 - Program coded.                                        *
 *                                                                   *
 *  Rick Fishman                                                     *
 *  Code Blazers, Inc.                                               *
 *  4113 Apricot                                                     *
 *  Irvine, CA. 92720                                                *
 *  CIS ID: 72251,750                                                *
 *                                                                   *
 *                                                                   *
 *********************************************************************/

/*********************************************************************/
/*------- Include relevant sections of the OS/2 header files --------*/
/*********************************************************************/

#define  INCL_DEV
#define  INCL_DOSFILEMGR
#define  INCL_DOSPROCESS
#define  INCL_WINDIALOGS
#define  INCL_WINERRORS
#define  INCL_WINFRAMEMGR
#define  INCL_WININPUT
#define  INCL_WINMENUS
#define  INCL_WINPOINTERS
#define  INCL_WINSTDCNR
#define  INCL_WINSTDDLGS
#define  INCL_WINSTDDRAG
#define  INCL_WINSYS
#define  INCL_WINWINDOWMGR

/**********************************************************************/
/*----------------------------- INCLUDES -----------------------------*/
/**********************************************************************/

#include <os2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cnradv.h"

/*********************************************************************/
/*------------------- APPLICATION DEFINITIONS -----------------------*/
/*********************************************************************/

#define DRAG_RMF  "(DRM_OS2FILE,DRM_PRINT,DRM_DISCARD)x(DRF_UNKNOWN)"

#define DOC_NAME  "CnrAdv"

/**********************************************************************/
/*---------------------------- STRUCTURES ----------------------------*/
/**********************************************************************/

/**********************************************************************/
/*----------------------- FUNCTION PROTOTYPES ------------------------*/
/**********************************************************************/

static INT     CountSelectedRecs   ( HWND hwndClient, PCNRITEM pciUnderMouse,
                                     PBOOL pfProcessSelected );
static VOID    SetSelectedDragItems( HWND hwndClient, PINSTANCE pi,
                                     INT cRecs, PDRAGINFO pdinfo,
                                     PDRAGIMAGE pdimg );
static VOID    SetOneDragItem      ( HWND hwndClient, PINSTANCE pi,
                                     PCNRITEM pciUnderMouse, PDRAGINFO pdinfo,
                                     PDRAGIMAGE pdimg, INT iOffset );
static VOID    OurOwnDrag          ( HWND hwndClient, PCNRITEM pciUnderMouse,
                                     PDRAGINFO pdinfo );
static VOID    MoveRecords         ( HWND hwndCnr, PCNRITEM pciUnderMouse,
                                     PCNRITEM *apciToMove, USHORT cRecs );
static VOID    SomeoneElsesDrag    ( HWND hwndClient, PCNRITEM pci,
                                     PDRAGINFO pdinfo );
static VOID    FillInNewRecord     ( PCNRITEM pci, PDRAGITEM pditem );
static MRESULT PrintObject         ( HWND hwndClient, PDRAGITEM pdi,
                                     PPRINTDEST ppd );
static VOID    FormatDirListing    ( PSZ pszDirListing, PCNRITEM pci );
static MRESULT DiscardObjects      ( HWND hwndClient, PDRAGINFO pdinfo );
static BOOL    WindowInOurProcess  ( HWND hwndClient, HWND hwndSource );
static VOID    RemoveSourceEmphasis( HWND hwndClient );


/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*--------------------------- DragMessage ----------------------------*/
/*                                                                    */
/*  A DM_ MESSAGE WAS RECEIVED BY THE CLIENT WINDOW PROCEDURE.        */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         message id,                                                */
/*         mp1 of the message,                                        */
/*         mp2 of the message                                         */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: return code of message                                    */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
MRESULT DragMessage( HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case DM_PRINTOBJECT:

            return PrintObject( hwndClient, (PDRAGITEM) mp1, (PPRINTDEST) mp2 );


        case DM_DISCARDOBJECT:

            return DiscardObjects( hwndClient, (PDRAGINFO) mp1 );


        case DM_DRAGERROR:

            return (MRESULT) DME_IGNOREABORT;


        // These are the rest of the DM_ messages that we don't currently
        // process

        case DM_DRAGFILECOMPLETE:
        case DM_DRAGLEAVE:
        case DM_DRAGOVER:
        case DM_DRAGOVERNOTIFY:
        case DM_DROP:
        case DM_DROPHELP:
        case DM_EMPHASIZETARGET:
        case DM_ENDCONVERSATION:
        case DM_FILERENDERED:
        case DM_RENDER:
        case DM_RENDERCOMPLETE:
        case DM_RENDERFILE:
        case DM_RENDERPREPARE:

            return 0;
    }

    return 0;
}

/**********************************************************************/
/*----------------------------- DragInit -----------------------------*/
/*                                                                    */
/*  PROCESS CN_INITDRAG NOTIFY MESSAGE.                               */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the CNRDRAGINIT structure                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID DragInit( HWND hwndClient, PCNRDRAGINIT pcdi )
{
    PCNRITEM    pciUnderMouse = (PCNRITEM) pcdi->pRecord;
    PDRAGIMAGE  pdimg = NULL;
    PDRAGINFO   pdinfo = NULL;
    BOOL        fProcessSelected;
    PINSTANCE   pi = INSTDATA( hwndClient );
    INT         cRecs;

    if( !pi )
    {
        Msg( "DragInit cant get Inst data RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Count the records that have CRA_SELECTED emphasis. Also return whether
    // or not we should process the CRA_SELECTED records. If the container
    // record under the mouse does not have this emphasis, we shouldn't.

    cRecs = CountSelectedRecs( hwndClient, pciUnderMouse, &fProcessSelected );

    if( cRecs )
    {
        INT iDragImageArraySize = cRecs * sizeof ( DRAGIMAGE );

        // Allocate an array of DRAGIMAGE structures. Each structure contains
        // info about an image that will be under the mouse pointer during the
        // drag. This image will represent a container record being dragged.

        pdimg = (PDRAGIMAGE) malloc( iDragImageArraySize );

        if( pdimg )
            (void) memset( pdimg, 0, iDragImageArraySize );
        else
            Msg( "Out of memory in CnrInitDrag" );

        // Let PM allocate enough memory for a DRAGINFO structure as well as
        // a DRAGITEM structure for each record being dragged. It will allocate
        // shared memory so other processes can participate in the drag/drop.

        pdinfo = DrgAllocDraginfo( cRecs );

        if( !pdinfo )
            Msg( "DrgAllocDraginfo failed. RC(%X)", HWNDERR( hwndClient ) );
    }

    if( cRecs && pdinfo && pdimg )
    {
        // Set the data from the container records into the DRAGITEM and
        // DRAGIMAGE structures. If we are to process CRA_SELECTED container
        // records, do them all in one function. If not, pass a pointer to the
        // container record under the mouse to a different function that will
        // fill in just one of each DRAGITEM / DRAGIMAGE structures.

        if( fProcessSelected )
            SetSelectedDragItems( hwndClient, pi, cRecs, pdinfo, pdimg );
        else
            SetOneDragItem( hwndClient, pi, pciUnderMouse, pdinfo, pdimg, 0 );

        // If DrgDrag returns NULLHANDLE, that means the user hit Esc or F1
        // while the drag was going on so the target didn't have a chance to
        // delete the string handles. So it is up to the source window to do
        // it. Unfortunately there doesn't seem to be a way to determine
        // whether the NULLHANDLE means Esc was pressed as opposed to there
        // being an error in the drag operation. So we don't attempt to figure
        // that out. To us, a NULLHANDLE means Esc was pressed...

        if( !DrgDrag( hwndClient, pdinfo, pdimg, cRecs, VK_ENDDRAG, NULL ) )
            if( !DrgDeleteDraginfoStrHandles( pdinfo ) )
                Msg( "DragInit DrgDeleteDraginfoStrHandles RC(%X)",
                     HWNDERR( hwndClient ) );

        // Take off source emphasis from the records that were dragged

        RemoveSourceEmphasis( hwndClient );
    }

    if( pdimg )
        free( pdimg );

    if( pdinfo )
        if( !DrgFreeDraginfo( pdinfo ) )
            Msg( "DragInit DrgFreeDraginfo RC(%X)", HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*------------------------ CountSelectedRecs -------------------------*/
/*                                                                    */
/*  COUNT THE NUMBER OF RECORDS THAT ARE CURRENTLY SELECTED.          */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the record that was under the pointer,          */
/*         address of BOOL - should we process selected records?      */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: number of records to process                              */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static INT CountSelectedRecs( HWND hwndClient, PCNRITEM pciUnderMouse,
                              PBOOL pfProcessSelected )
{
    INT      cRecs = 0;
    PCNRITEM pci;

    *pfProcessSelected = FALSE;

    // If the record under the mouse is NULL, we must be over whitespace, in
    // which case we don't want to drag any records.

    if( pciUnderMouse )
    {
        pci = (PCNRITEM) CMA_FIRST;

        // Count the records with 'selection' emphasis. These are the records
        // we want to drag, unless the container record under the mouse does
        // not have selection emphasis. If that is the case, we only want to
        // process that one.

        while( pci )
        {
            pci = (PCNRITEM) WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY,
                                                CM_QUERYRECORDEMPHASIS,
                                                MPFROMP( pci ),
                                                MPFROMSHORT( CRA_SELECTED ) );

            if( pci == (PCNRITEM) -1 )
                Msg( "CountSelectedRecs..CM_QUERYRECORDEMPHASIS RC(%X)",
                     HWNDERR( hwndClient ) );
            else if( pci )
            {
                if( pci == pciUnderMouse )
                    *pfProcessSelected = TRUE;

                cRecs++;
            }
        }

        if( !(*pfProcessSelected) )
            cRecs = 1;
    }

    return cRecs;
}

/**********************************************************************/
/*----------------------- SetSelectedDragItems -----------------------*/
/*                                                                    */
/*  FILL THE DRAGINFO STRUCT WITH DRAGITEM STRUCTS FOR SELECTED RECS. */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to client window's instance data,                  */
/*         count of selected records,                                 */
/*         pointer to allocated DRAGINFO struct,                      */
/*         pointer to allocated DRAGIMAGE array                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID SetSelectedDragItems( HWND hwndClient, PINSTANCE pi, INT cRecs,
                                  PDRAGINFO pdinfo, PDRAGIMAGE pdimg )
{
    PCNRITEM pci;
    INT      i;

    pci = (PCNRITEM) CMA_FIRST;

    for( i = 0; i < cRecs; i++, pdimg++ )
    {
        pci = (PCNRITEM) WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY,
                                            CM_QUERYRECORDEMPHASIS,
                                            MPFROMP( pci ),
                                            MPFROMSHORT( CRA_SELECTED ) );

        if( pci == (PCNRITEM) -1 )
            Msg( "SetSelectedDragItems..CM_QUERYRECORDEMPHASIS RC(%X)",
                 HWNDERR( hwndClient ) );
        else
            SetOneDragItem( hwndClient, pi, pci, pdinfo, pdimg, i );
    }
}

/**********************************************************************/
/*-------------------------- SetOneDragItem --------------------------*/
/*                                                                    */
/*  SET ONE DRAGITEM STRUCT INTO A DRAGINFO STRUCT.                   */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to client window's instance data,                  */
/*         pointer to CNRITEM that contains current container record, */
/*         pointer to allocated DRAGINFO struct,                      */
/*         pointer to allocated DRAGIMAGE array,                      */
/*         record offset into DRAGINFO struct to place DRAGITEM       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID SetOneDragItem( HWND hwndClient, PINSTANCE pi, PCNRITEM pci,
                            PDRAGINFO pdinfo, PDRAGIMAGE pdimg, INT iOffset )
{
    DRAGITEM ditem;

    (void) memset( &ditem, 0, sizeof( DRAGITEM ) );

    // Fill in the DRAGITEM struct

    ditem.hwndItem  = hwndClient;    // Window handle of the source of the drag
    ditem.ulItemID  = (ULONG) pci;   // We use this to store the container rec
    ditem.hstrType  = DrgAddStrHandle( DRT_UNKNOWN ); // Application defined
    ditem.hstrRMF   = DrgAddStrHandle( DRAG_RMF ); // Defined at top of module
    ditem.hstrContainerName = DrgAddStrHandle( pi->szDirectory );
    ditem.hstrSourceName = DrgAddStrHandle( pci->szFileName );
    ditem.hstrTargetName = ditem.hstrSourceName;
    ditem.fsSupportedOps = DO_COPYABLE;  // We support copy only

    // Set the DRAGITEM struct into the memory allocated by
    // DrgAllocDraginfo()

    DrgSetDragitem( pdinfo, &ditem, sizeof( DRAGITEM ), iOffset );

    // Fill in the DRAGIMAGE structure

    pdimg->cb       = sizeof( DRAGIMAGE );
    pdimg->hImage   = pci->hptrIcon;        // Image under mouse during drag
    pdimg->fl       = DRG_ICON;             // hImage is an HPOINTER
    pdimg->cxOffset = 5 * iOffset;          // Image offset from mouse pointer
    pdimg->cyOffset = 5 * iOffset;          // Image offset from mouse pointer

    // Set source emphasis for this container record

    if( !WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY, CM_SETRECORDEMPHASIS,
                            MPFROMP( pci ), MPFROM2SHORT( TRUE, CRA_SOURCE ) ) )
        Msg( "SetOneDragItem..CM_SETRECORDEMPHASIS RC(%X)",
             HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*----------------------------- DragOver -----------------------------*/
/*                                                                    */
/*  PROCESS CN_DRAGOVER NOTIFY MESSAGE.                               */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the CNRDRAGINFO structure                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: return value from CN_DRAGOVER processing                  */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
MRESULT DragOver( HWND hwndClient, PCNRDRAGINFO pcdi )
{
    USHORT    usDrop, usDefaultOp;
    PCNRITEM  pci = (PCNRITEM) pcdi->pRecord;
    PDRAGINFO pdinfo = pcdi->pDragInfo;
    PDRAGITEM pditem;
    INT       i;
    PINSTANCE pi = INSTDATA( hwndClient );

    if( !pi )
    {
        Msg( "DragOver cant get Inst data RC(%X)", HWNDERR( hwndClient ) );

        return MRFROM2SHORT( DOR_NEVERDROP, 0 );
    }

    // Don't allow a drop over a record that is not DROPONABLE if we are in
    // icon view. In the other views it makes sense to be dropping over another
    // record since that is how you indicate where you want the record to be
    // inserted. In icon view the user should insert over white space.

    if( pi->flCurrentView == CV_ICON && pci &&
        !(pci->rc.flRecordAttr & CRA_DROPONABLE) )
        return MRFROM2SHORT( DOR_NODROP, 0 );

    if( !DrgAccessDraginfo( pdinfo ) )
    {
        Msg( "DragOver DrgAccessDraginfo RC(%X)", HWNDERR( hwndClient ) );

        return MRFROM2SHORT( DOR_NEVERDROP, 0 );
    }

    // Don't allow a drop if one of the dropped records is the same record
    // that's under the mouse pointer. That causes problems in this app,
    // although it may be appropriate in other applications.

    if( pci )
        for( i = 0; i < pdinfo->cditem; i++ )
        {
            pditem = DrgQueryDragitemPtr( pdinfo, i );

            if( pci == (PCNRITEM) pditem->ulItemID )
                return MRFROM2SHORT( DOR_NODROP, 0 );
        }

    pditem = DrgQueryDragitemPtr( pdinfo, 0 );

    if( pditem )
    {
        // We will only allow OS2 FILES to be dropped on us

        if( DrgVerifyRMF( pditem, "DRM_OS2FILE", NULL ) )
        {
            usDrop = DOR_DROP;

            // If we are the source as well as the target, the default operation
            // will be to MOVE the record. Otherwise it will be to COPY it

            if( pdinfo->hwndSource == hwndClient )
                usDefaultOp = DO_MOVE;
            else
                usDefaultOp = DO_COPY;
        }
        else
        {
            usDrop = DOR_NEVERDROP;

            usDefaultOp = 0;
        }
    }
    else
        Msg( "DragOver DrgQueryDragitemPtr RC(%X)", HWNDERR( hwndClient ) );

    // Free our handle to the shared memory if the source window is not in our
    // process.

    if( !WindowInOurProcess( hwndClient, pdinfo->hwndSource ) )
        if( !DrgFreeDraginfo( pdinfo ) )
            Msg( "DragOver DrgFreeDraginfo RC(%X)", HWNDERR( hwndClient ) );

    return MRFROM2SHORT( usDrop, usDefaultOp );
}

/**********************************************************************/
/*---------------------------- DragDrop ------------------------------*/
/*                                                                    */
/*  PROCESS CN_DROP NOTIFY MESSAGE.                                   */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the CNRDRAGINFO structure                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID DragDrop( HWND hwndClient, PCNRDRAGINFO pcdi )
{
    PDRAGINFO pdinfo = pcdi->pDragInfo;

    if( !DrgAccessDraginfo( pdinfo ) )
    {
        Msg( "DragDrop DrgAccessDraginfo RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    if( pdinfo->hwndSource == hwndClient )
        OurOwnDrag( hwndClient, (PCNRITEM) pcdi->pRecord, pdinfo );
    else
        SomeoneElsesDrag( hwndClient, (PCNRITEM) pcdi->pRecord, pdinfo );

    // The docs say to do this after a DM_DROP or DM_DROPHELP message but they
    // don't say why. I guess we'll do as we're told <g>.

    if( !DrgDeleteDraginfoStrHandles( pdinfo ) )
        Msg( "DragDrop DrgDeleteDraginfoStrHandles RC(%X)",
             HWNDERR( hwndClient ) );

    // Free the shared memory we got access to using DrgAccessDragInfo but only
    // if the source window is not in our process

    if( !WindowInOurProcess( hwndClient, pdinfo->hwndSource ) )
        if( !DrgFreeDraginfo( pdinfo ) )
            Msg( "DragDrop DrgFreeDraginfo RC(%X)", HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*--------------------------- OurOwnDrag -----------------------------*/
/*                                                                    */
/*  PROCESS CN_DROP NOTIFY MESSAGE COMING FROM OUR OWN CONTAINER      */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the container record under the mouse,           */
/*         pointer to the DRAGINFO structure                          */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID OurOwnDrag( HWND hwndClient, PCNRITEM pciUnderMouse,
                        PDRAGINFO pdinfo )
{
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    PDRAGITEM pditem;
    INT       i, iSpacing = 0;
    RECTL     rclPrev;
    PCNRITEM  *apci;
    QUERYRECORDRECT qrr;
    PINSTANCE pi = INSTDATA( hwndClient );
    BOOL      fSuccess;

    // Allocate memory for an array of container record pointers. This array
    // will be used during the CM_INVALIDATE message to invalidate all dropped
    // records in one shot

    apci = (PCNRITEM *) malloc( pdinfo->cditem * sizeof( PCNRITEM ) );

    if( !apci )
    {
        Msg( "Out of memory in OurOwnDrag!" );

        return;
    }

    (void) memset( &qrr, 0, sizeof( QUERYRECORDRECT ) );

    // Go thru each dropped record and erase it from its present place in the
    // container, then invalidate it so it repaints itself in its new spot.
    // This is really only valid in Icon view.

    for( i = 0; i < pdinfo->cditem; i++ )
    {
        pditem = DrgQueryDragitemPtr( pdinfo, i );

        if( !pditem )
        {
            Msg( "OurOwnDrag DrgQueryDragitemPtr RC(%X)", HWNDERR( hwndCnr ) );

            break;
        }

        // Get the CNRITEM pointer that we stuck in ulItemID in DragInit

        apci[ i ] = (PCNRITEM) pditem->ulItemID;

        // Remove source emphasis from the dropped record

        if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS,
                         MPFROMP( apci[ i ] ),
                         MPFROM2SHORT( FALSE, CRA_SOURCE ) ) )
            Msg( "OurOwnDrag CM_SETRECORDEMPHASIS RC(%X)", HWNDERR( hwndCnr ) );

        // Get the current position of the container record before it is moved

        qrr.cb                  = sizeof( QUERYRECORDRECT );
        qrr.pRecord             = (PRECORDCORE) apci[ i ];
        qrr.fsExtent            = CMA_ICON | CMA_TEXT | CMA_TREEICON;
        qrr.fRightSplitWindow   = FALSE;

        if( !WinSendMsg( hwndCnr, CM_QUERYRECORDRECT, MPFROMP( &rclPrev ),
                         MPFROMP( &qrr ) ) )
            Msg( "OurOwnDrag CM_QUERYRECORDRECT RC(%X)", HWNDERR( hwndCnr ) );

        // If Icon view, erase the visible record from the container
        // (this doesn't remove the record). If not icon view, it doesn't make
        // a difference because there shouldn't be holes left.

        if( pi && pi->flCurrentView == CV_ICON )
            if( !WinSendMsg( hwndCnr, CM_ERASERECORD, MPFROMP( apci[ i ] ),
                             NULL ) )
                Msg( "OurOwnDrag CM_ERASERECORD RC(%X)", HWNDERR( hwndCnr ) );

        // Find out where the mouse is in relation to the container and set the
        // container record's new position as that point plus an offset from it.
        // In other words, we are moving the icon.

        if( !WinQueryPointerPos( HWND_DESKTOP, &(apci[ i ]->rc.ptlIcon) ) )
            Msg( "OurOwnDrag WinQueryPointerPos RC(%X)", HWNDERR( hwndCnr ) );

        if( !WinMapWindowPoints( HWND_DESKTOP, hwndCnr,
                                 &(apci[ i ]->rc.ptlIcon), 1 ) )
            Msg( "OurOwnDrag WinMapWindowPoints RC(%X)", HWNDERR( hwndCnr ) );

        if( i )
        {
            iSpacing += ((rclPrev.xRight - rclPrev.xLeft) + 5);

            apci[ i ]->rc.ptlIcon.x += iSpacing;
        }
    }

    // If the mouse was over a container record when the drop ocurred, move
    // the records to their new position. This is accomplished by removing the
    // records and then re-inserting them.

    if( pciUnderMouse )
        MoveRecords( hwndCnr, pciUnderMouse, apci, pdinfo->cditem );

    // If we are in Icon view, only invalidate the records that have been moved.
    // This eliminates flicker. If not in icon view, if we do this, holes are
    // left that look strange. For instance, a hole left in Tree view where the
    // old records were is strange to say the least. So if not in icon view,
    // invalidate the whole container. This will cause the records to look like
    // we want them to look on a move.

    if( pi && pi->flCurrentView == CV_ICON )
        fSuccess = (BOOL) WinSendMsg( hwndCnr, CM_INVALIDATERECORD,
                   MPFROMP( apci ),
                   MPFROM2SHORT( pdinfo->cditem, CMA_REPOSITION ) );
    else
        fSuccess = (BOOL) WinSendMsg( hwndCnr, CM_INVALIDATERECORD,
                   MPFROMP( apci ),
                   MPFROM2SHORT( 0, CMA_REPOSITION ) );

    if( !fSuccess )
        Msg( "OurOwnDrag CM_INVALIDATERECORD RC(%X)", HWNDERR( hwndCnr ) );

    free( apci );
}

/**********************************************************************/
/*--------------------------- MoveRecords ----------------------------*/
/*                                                                    */
/*  MOVE RECORDS BY REMOVING THEM AND RE-INSERTING THEM.              */
/*                                                                    */
/*  INPUT: container window handle,                                   */
/*         pointer to container record under the mouse,               */
/*         array of container record pointers to move,                */
/*         number of records to move                                  */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID MoveRecords( HWND hwndCnr, PCNRITEM pciUnderMouse,
                         PCNRITEM *apciToMove, USHORT cRecsToMove )
{
    RECORDINSERT ri;
    LONG         cRecsLeft;
    PCNRITEM     pciParent = NULL;
    INT          i;

    // First remove the records from the container

    cRecsLeft = (LONG) WinSendMsg( hwndCnr, CM_REMOVERECORD,
                                   MPFROMP( apciToMove ),
                                   MPFROM2SHORT( cRecsToMove, 0 ) );

    if( cRecsLeft == -1 )
    {
        Msg( "MoveRecords CM_REMOVERECORD RC(%X)", HWNDERR( hwndCnr ) );

        return;
    }

    // Get the parent of the record that is under the mouse. Since we will be
    // inserting the records after the record under the mouse, we need to set
    // the new records' parent to be the same as that one's. If the mouse is
    // at the top of the container, we will get CMA_FIRST, in which case we
    // don't want to issue the CM_QUERYRECORD message as we will trap with
    // CMA_FIRST being used as a record pointer.

    if( pciUnderMouse != (PCNRITEM) CMA_FIRST )
        pciParent = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORD,
                                       MPFROMP( pciUnderMouse ),
                                       MPFROM2SHORT(CMA_PARENT, CMA_ITEMORDER));

    if( pciParent == (PCNRITEM) -1 )
        Msg( "MoveRecords CM_QUERYRECORD RC(%X)", HWNDERR( hwndCnr ) );

    // Insert the new records after the one under the mouse. Note that
    // CM_INSERTRECORD does not take in mp1 an array of record pointers like the
    // CM_REMOVERECORD message does. It takes an array of RECORDCORE structures.
    // We must insert them one at a time because if you want to insert more
    // than one at a time you must be inserting a linked list allocated by
    // CM_ALLOCRECORD. Since the CM_REMOVERECORD message does not free the
    // records we use the same record memory to insert them back into the
    // container.

    (void) memset( &ri, 0, sizeof( RECORDINSERT ) );

    ri.cb                 = sizeof( RECORDINSERT );
    ri.pRecordOrder       = (PRECORDCORE) pciUnderMouse;
    ri.pRecordParent      = (PRECORDCORE) pciParent;
    ri.zOrder             = (USHORT) CMA_TOP;
    ri.cRecordsInsert     = 1;
    ri.fInvalidateRecord  = FALSE;

    for( i = 0; i < cRecsToMove; i++ )
    {
        if( !WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP( apciToMove[ i ] ),
                         MPFROMP( &ri ) ) )
            Msg( "MoveRecords CM_INSERTRECORD RC(%X)", HWNDERR( hwndCnr ) );

        // We want to insert each record after the prior one

        ri.pRecordOrder = (PRECORDCORE) apciToMove[ i ];
    }
}

/**********************************************************************/
/*------------------------ SomeoneElsesDrag --------------------------*/
/*                                                                    */
/*  PROCESS CN_DROP NOTIFY MESSAGE COMING FROM OTHER THAN OUR WINDOW  */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to the container record under the mouse,           */
/*         pointer to the DRAGINFO structure                          */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID SomeoneElsesDrag( HWND hwndClient, PCNRITEM pciUnderMouse,
                              PDRAGINFO pdinfo )
{
    HWND         hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    PDRAGITEM    pditem;
    INT          i;
    PCNRITEM     pci, pciFirst, pciParent = NULL, pciRecordAfter;
    RECORDINSERT ri;

    // Let the container allocate memory for the new container records that
    // were dropped on it.

    pci = pciFirst = WinSendMsg( hwndCnr, CM_ALLOCRECORD,
                                 MPFROMLONG( EXTRA_RECORD_BYTES ),
                                 MPFROMLONG( pdinfo->cditem ) );

    if( !pciFirst )
    {
        Msg( "SomeoneElsesDrag CM_ALLOCRECORD RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Go thru each dropped record and add it to the linked list of records
    // that will be added to the container

    for( i = 0; i < pdinfo->cditem; i++ )
    {
        pditem = DrgQueryDragitemPtr( pdinfo, i );

        if( !pditem )
        {
            Msg( "SomeoneElsesDrag DrgQueryDragitemPtr RC(%X)",
                 HWNDERR( hwndCnr ) );

            break;
        }

        FillInNewRecord( pci, pditem );

        // Find out where the mouse is in relation to the container and set the
        // container record's new position as that point plus an offset from it.
        // In other words, we are moving the icon.

        if( !WinQueryPointerPos( HWND_DESKTOP, &(pci->rc.ptlIcon) ) )
            Msg( "OurOwnDrag WinQueryPointerPos RC(%X)", HWNDERR( hwndCnr ) );

        if( !WinMapWindowPoints( HWND_DESKTOP, hwndCnr,
                                 &(pci->rc.ptlIcon), 1 ) )
            Msg( "OurOwnDrag WinMapWindowPoints RC(%X)", HWNDERR( hwndCnr ) );

        if( i )
            pci->rc.ptlIcon.x += 40;

        // Let the source know that we are done rendering this item

        DrgSendTransferMsg( pditem->hwndItem, DM_ENDCONVERSATION,
                            MPFROMLONG( pditem->ulItemID ),
                            MPFROMLONG( DMFL_TARGETSUCCESSFUL ) );

        pci = (PCNRITEM) pci->rc.preccNextRecord;
    }

    // If there was a record under the mouse when records were dropped...

    if( pciUnderMouse )
    {
        pciRecordAfter = pciUnderMouse;

        // Get the parent of the record that is under the mouse. Since we will
        // be inserting the records after the record under the mouse, we need
        // to set the new records' parent to be the same as that one's.

        pciParent = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORD,
                                    MPFROMP( pciUnderMouse ),
                                    MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );

        if( pciParent == (PCNRITEM) -1 )
            Msg( "SomeOneElsesDrag CM_QUERYRECORD RC(%X)", HWNDERR( hwndCnr ) );
    }
    else
        pciRecordAfter = (PCNRITEM) CMA_END;

    // Insert the container records. If there was a record under the mouse when
    // the records were dropped, insert them after that one. Otherwise insert
    // them at the end of the container's linked list.

    (void) memset( &ri, 0, sizeof( RECORDINSERT ) );

    ri.cb                 = sizeof( RECORDINSERT );
    ri.pRecordOrder       = (PRECORDCORE) pciRecordAfter;
    ri.pRecordParent      = (PRECORDCORE) pciParent;
    ri.zOrder             = (USHORT) CMA_TOP;
    ri.cRecordsInsert     = pdinfo->cditem;
    ri.fInvalidateRecord  = TRUE;

    if( !WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP( pciFirst ),
                     MPFROMP( &ri ) ) )
        Msg( "SomeoneElsesDrag CM_INSERTRECORD RC(%X)", HWNDERR( hwndCnr ) );
}

/**********************************************************************/
/*------------------------ FillInNewRecord ---------------------------*/
/*                                                                    */
/*  POPULATE CONTAINER RECORD WITH FILE INFORMATION                   */
/*                                                                    */
/*  INPUT: pointer to record buffer to fill,                          */
/*         pointer to DRAGITEM structure of dragged record            */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID FillInNewRecord( PCNRITEM pci, PDRAGITEM pditem )
{
    CHAR        szSource[ CCHMAXPATH + 1 ];
    PCH         pch;
    HPOINTER    hptr;
    FILESTATUS3 fs;
    APIRET      rc;

    // DrgQueryStrName gets access to a drag/drop shared-memory string
    // Get the fully-qualified source filename into szSource

    DrgQueryStrName( pditem->hstrContainerName, sizeof( szSource ), szSource );

    pch = szSource + strlen( szSource );

    if( pch > szSource && *(pch - 1) != '\\' )
        *(pch)++ = '\\';

    DrgQueryStrName( pditem->hstrSourceName,
                     sizeof( szSource ) - strlen( szSource ), pch );

    // If hstrTargetName is non-NULL, that means the source of the drag wants
    // that to be the name of the file. If it is NULL, source-name is to be
    // used as the file name.

    DrgQueryStrName( pditem->hstrTargetName, sizeof( pci->szFileName ),
                     pci->szFileName );

    if( !(*pci->szFileName) )
        DrgQueryStrName( pditem->hstrSourceName, sizeof( pci->szFileName ),
                         pci->szFileName );

    // Get file information

    rc = DosQueryPathInfo( szSource, FIL_STANDARD, &fs, sizeof( FILESTATUS3 ) );

    if( rc )
    {
        (void) memset( &fs, 0, sizeof( FILESTATUS3 ) );

        Msg( "FillInNewRecord DosQueryPathInfo RC(%u)", rc );
    }

    // Get the icon associated with the fully qualified source filename. If we
    // are talking about a directory, use the folder icon we loaded at program
    // init time. WinLoadFileIcon does not differentiate between regular files
    // and folders.

    if( fs.attrFile & FILE_DIRECTORY )
        hptr = hptrFolder;
    else
        hptr = WinLoadFileIcon( szSource, FALSE );

    // Fill in the file information.

    pci->pszFileName    = pci->szFileName;
    pci->hptrIcon       = hptr;
    pci->date.day       = fs.fdateLastWrite.day;
    pci->date.month     = fs.fdateLastWrite.month;
    pci->date.year      = fs.fdateLastWrite.year;
    pci->time.seconds   = fs.ftimeLastWrite.twosecs;
    pci->time.minutes   = fs.ftimeLastWrite.minutes;
    pci->time.hours     = fs.ftimeLastWrite.hours;
    pci->cbFile         = fs.cbFile;
    pci->attrFile       = fs.attrFile;
    pci->iDirPosition   = 0;
    pci->rc.pszIcon     = pci->pszFileName;
    pci->rc.hptrIcon    = hptr;
}

/**********************************************************************/
/*--------------------------- PrintObject ----------------------------*/
/*                                                                    */
/*  PROCESS DM_PRINTOBJECT MESSAGE.                                   */
/*                                                                    */
/*  NOTE: PM sends us one of these messages for each item dropped on  */
/*        the printer object.  Go figure! This is not documented      */
/*        anywhere as far as I know.                                  */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to DRAGITEM structure,                             */
/*         pointer to PRINTDEST structure                             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: DRR_SOURCE or DRR_TARGET                                  */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static MRESULT PrintObject( HWND hwndClient, PDRAGITEM pditem, PPRINTDEST ppd )
{
    HAB      hab = ANCHOR( hwndClient );
    PCNRITEM pci = (PCNRITEM) pditem->ulItemID;
    HDC      hdc;

    // Get a printer device context using the information returned from PM
    // in the PRINTDEST structure

    hdc = DevOpenDC( hab, ppd->lType, ppd->pszToken, ppd->lCount, ppd->pdopData,
                     NULLHANDLE );

    if( pci && hdc )
    {
        LONG lBytes, lError;

        // If PM is telling us that we should let the user decide on job
        // properties, we accommodate it by putting up the job properties
        // dialog box.

        if( ppd->fl & PD_JOB_PROPERTY )
            DevPostDeviceModes( hab, ((PDEVOPENSTRUC) ppd->pdopData)->pdriv,
                                ((PDEVOPENSTRUC)ppd->pdopData)->pszDriverName,
                                ppd->pszPrinter, NULL, DPDM_POSTJOBPROP );

        // Tell the spooler that we are starting a document.

        lError = DevEscape( hdc, DEVESC_STARTDOC, strlen( DOC_NAME ),
                            DOC_NAME, &lBytes, NULL );

        if( lError == DEV_OK )
        {
            CHAR szDirListing[ 80 ];

            FormatDirListing( szDirListing, pci );

            // Write the directory listing of the file to the printer

            lError = DevEscape( hdc, DEVESC_RAWDATA, strlen( szDirListing ),
                                szDirListing, &lBytes, NULL );

            if( lError == DEVESC_ERROR )
                Msg( "Bad DevEscape RAWDATA. RC(%X)", HWNDERR( hwndClient ) );

            // Tell the spooler that we are done printing this document

            lError = DevEscape( hdc, DEVESC_ENDDOC, 0, NULL, &lBytes, NULL );

            if( lError == DEVESC_ERROR )
                Msg( "Bad DevEscape ENDDOC. RC(%X)", HWNDERR( hwndClient ) );
        }
        else
            Msg( "Bad DevEscape STARTDOC for %s. RC(%X)", DOC_NAME,
                 HWNDERR( hwndClient ) );
    }

    if( hdc )
        DevCloseDC( hdc );
    else
        Msg( "DevOpenDC failed. RC(%X)", HWNDERR( hwndClient ) );

    // Tell PM that we are doing the printing. We could return DRR_TARGET
    // which would leave the printing burden on the printer object or DRR_ABORT
    // which would tell PM to abort the drop.

    return (MRESULT) DRR_SOURCE;
}

/**********************************************************************/
/*------------------------ FormatDirListing --------------------------*/
/*                                                                    */
/*  FORMAT A DIRECTORY LISTING FOR THE FILE BEING PRINTED.            */
/*                                                                    */
/*  INPUT: pointer to buffer to hold formatted directory listing,     */
/*         pointer to CNRITEM container record                        */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID FormatDirListing( PSZ pszDirListing, PCNRITEM pci )
{
    CNRITEM ci = *pci;
    CHAR    chAmPm, szSizeOrDir[ 15 ];
    INT     iYear;

    if( ci.time.hours == 0 )
    {
        ci.time.hours = 12;
        chAmPm = 'a';
    }
    else if( ci.time.hours == 12 )
        chAmPm = 'p';
    else if( ci.time.hours < 12 )
        chAmPm = 'a';
    else
    {
        ci.time.hours -= 12;
        chAmPm = 'p';
    }

    if( pci->attrFile & FILE_DIRECTORY )
        (void) strcpy( szSizeOrDir, "      <DIR>   " );
    else
        (void) sprintf( szSizeOrDir, "%14ld", pci->cbFile );

    iYear = ci.date.year + 1980;

    iYear -= ( iYear >= 2000 ? 2000 : 1900 );

    sprintf( pszDirListing, "%2u-%02u-%02u  %2u:%02u%c %s  %-40.40s\n",
            ci.date.month, ci.date.day, iYear, ci.time.hours, ci.time.minutes,
            chAmPm, szSizeOrDir, pci->szFileName );
}

/**********************************************************************/
/*-------------------------- DiscardObjects --------------------------*/
/*                                                                    */
/*  PROCESS DM_DISCARDOBJECT MESSAGE.                                 */
/*                                                                    */
/*  NOTE: We get a DM_DISCARDOBJECT message for each record being     */
/*        dropped. Since we get a DRAGINFO pointer the first time,    */
/*        we process all records the first time around. The rest of   */
/*        the times we go thru the motions but don't really do        */
/*        anything.                                                   */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to DRAGINFO structure                              */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: DRR_SOURCE or DRR_TARGET                                  */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static MRESULT DiscardObjects( HWND hwndClient, PDRAGINFO pdinfo )
{
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    INT       i;
    PDRAGITEM pditem;
    PCNRITEM  *apci;

    // Allocate memory for an array of container record pointers. This array
    // will be used during the CM_REMOVERECORD message to remove all dropped
    // records in one shot

    apci = (PCNRITEM *) malloc( pdinfo->cditem * sizeof( PCNRITEM ) );

    if( apci )
        (void) memset( apci, 0, pdinfo->cditem * sizeof( PCNRITEM ) );
    else
    {
        Msg( "Out of memory in DiscardObjects!" );

        return (MRESULT) DRR_ABORT;
    }

    if( apci )
    {
        INT iNumToDiscard = pdinfo->cditem;

        for( i = 0; i < pdinfo->cditem; i++ )
        {
            pditem = DrgQueryDragitemPtr( pdinfo, i );

            if( pditem )
                apci[ i ] = (PCNRITEM) pditem->ulItemID;
            else
            {
                iNumToDiscard--;

                Msg( "DiscardObjects DrgQueryDragitemPtr RC(%X)",
                     HWNDERR( hwndClient ) );
            }
        }

        if( iNumToDiscard )
        {
            // If CMA_FREE is used on CM_REMOVERECORD, the program traps on the
            // second DM_DISCARDOBJECT. So we can't blindly use CMA_FREE on the
            // CM_REMOVERECORD call. See comment below.

            INT cRecsLeft = (INT) WinSendMsg( hwndCnr, CM_REMOVERECORD,
                     MPFROMP( apci ),
                     MPFROM2SHORT( iNumToDiscard, CMA_INVALIDATE ) );

            // -1 means invalid parameter. The problem is that we get as many
            // DM_DISCARDOBJECT messages as there are records dragged but the
            // first one has enough info to process all of them. The messages
            // after the first are irrelevant because we already did the
            // removing. Since subsequent DM_DISCARDOBJECT messages will cause
            // the above to fail (records were already moved the first time),
            // we need to not try and free the records here. If we do, we'll
            // trap because they were freed the first time.

            if( cRecsLeft != -1 )
                WinSendMsg( hwndCnr, CM_FREERECORD, MPFROMP( apci ),
                            MPFROMSHORT( iNumToDiscard ) );
        }
    }
    else
        Msg( "Out of memory in DiscardObjects" );

    if( apci )
        free( apci );

    // Tell PM that we are doing the discarding. We could return DRR_TARGET
    // which would leave the discarding burden on the shredder object or
    // DRR_ABORT which would tell PM to abort the drop.

    return (MRESULT) DRR_SOURCE;
}

/**********************************************************************/
/*------------------------ WindowInOurProcess ------------------------*/
/*                                                                    */
/*  DETERMINE IF THE SOURCE WINDOW IS IN OUR PROCESS.                 */
/*                                                                    */
/*  INPUT: source window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if window is in our process                 */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL WindowInOurProcess( HWND hwndClient, HWND hwndSource )
{
    APIRET rc;
    PTIB   ptib;
    PPIB   ppib;
    PID    pid;
    TID    tid;
    BOOL   fSuccess, fOneOfOurs;

    // Get our Process ID

    rc = DosGetInfoBlocks( &ptib, &ppib );

    if( !rc )
    {
        // Get the source window's Process ID

        fSuccess = WinQueryWindowProcess( hwndSource, &pid, &tid );

        if( !fSuccess )
            Msg( "WinQueryWindowProcess RC(%X)", HWNDERR( hwndClient ) );
    }
    else
        Msg( "DosGetInfoBlocks RC(%X)", rc );

    if( !rc && fSuccess )
        fOneOfOurs = (pid == ppib->pib_ulpid) ? TRUE : FALSE;
    else
        fOneOfOurs = TRUE;

    return fOneOfOurs;
}

/**********************************************************************/
/*----------------------- RemoveSourceEmphasis -----------------------*/
/*                                                                    */
/*  REMOVE SOURCE EMPHASIS FROM THE DRAGGED RECORDS.                  */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID RemoveSourceEmphasis( HWND hwndClient )
{
    PCNRITEM pci = (PCNRITEM) CMA_FIRST;

    // For every record with source emphasis, remove it.

    while( pci )
    {
        pci = (PCNRITEM) WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY,
                                         CM_QUERYRECORDEMPHASIS,
                                         MPFROMP( pci ),
                                         MPFROMSHORT( CRA_SOURCE ) );

        if( pci == (PCNRITEM) -1 )
            Msg( "CountSelectedRecs..CM_QUERYRECORDEMPHASIS RC(%X)",
                 HWNDERR( hwndClient ) );
        else if( pci )
            if( !WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY,
                                    CM_SETRECORDEMPHASIS, MPFROMP( pci ),
                                    MPFROM2SHORT( FALSE, CRA_SOURCE ) ) )
                Msg( "RemoveSourceEmphasis..CM_SETRECORDEMPHASIS RC(%X)",
                     HWNDERR( hwndClient ) );
    }
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
