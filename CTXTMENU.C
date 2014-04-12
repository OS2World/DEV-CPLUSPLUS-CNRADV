/*********************************************************************
 *                                                                   *
 * MODULE NAME :  ctxtmenu.c             AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It contains the functions     *
 *  necessary to implement the container context menu.               *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  VOID CtxtmenuCreate( HWND hwndClient, PCNRITEM pciSelected );    *
 *  VOID CtxtmenuCommand( HWND hwndClient, ULONG idCommand );        *
 *  VOID CtxtmenuSetView( HWND hwndClient, ULONG ulViewType );       *
 *  VOID CtxtmenuEnd( HWND hwndClient );                             *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  11-30-92 - Source copied from CNRMENU.EXE sample.                *
 *             Added processing of selection-type menu items.        *
 *             Added processing of new items on View submenu.        *
 *             Added ToggleMiniIcons function.                       *
 *  12-05-92 - Added CA_OWNERPAINTBACKGROUND in CtxtmenuSetView.     *
 *             Added processing of Background menu items.            *
 *             Added SetViewType() to cut down CtxtmenuSetView code. *
 *             Added CA_MIXEDTARGETEMPH in CtxtmenuSetView.          *
 *                                                                   *
 *  Rick Fishman                                                     *
 *  Code Blazers, Inc.                                               *
 *  4113 Apricot                                                     *
 *  Irvine, CA. 92720                                                *
 *  CIS ID: 72251,750                                                *
 *                                                                   *
 *********************************************************************/

#pragma strings(readonly)   // used for debug version of memory mgmt routines

/*********************************************************************/
/*------- Include relevant sections of the OS/2 header files --------*/
/*********************************************************************/

#define  INCL_WINDIALOGS
#define  INCL_WINERRORS
#define  INCL_WINFRAMEMGR
#define  INCL_WINMENUS
#define  INCL_WINPOINTERS
#define  INCL_WINSTDCNR
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

/**********************************************************************/
/*---------------------------- STRUCTURES ----------------------------*/
/**********************************************************************/

/**********************************************************************/
/*----------------------- FUNCTION PROTOTYPES ------------------------*/
/**********************************************************************/

static VOID   SetViewType         ( ULONG ulViewType, PINSTANCE pi );
static VOID   TurnOffSelFlags     ( HWND hwndCnr );
static VOID   NewWindows          ( HWND hwndClient, BOOL fAllSelected );
static VOID   TurnOnSourceEmphasis( HWND hwndClient );
static VOID   TurnOffSourceEmphasis( HWND hwndClient );
static INT    CountSelectedRecs   ( HWND hwndCnr, PCNRITEM pciUnderMouse );
static VOID   NewWin              ( HWND hwndCnr, PSZ szBaseDir, PCNRITEM pci );
static VOID   TailorMenu          ( HWND hwndCnr, HWND hwndMenu,
                                    PCNRITEM pciSelected );
static VOID   SetConditionalCascade( HWND hwndMenu, USHORT idSubMenu,
                                    USHORT idDefaultItem );
static INT    AddOtherWindows     ( HWND hwndCnr, HWND hwndMenu );
static BOOL   AddOtherWinItem     ( HWND hwndClient, HWND hwndOtherFrame,
                                    HWND hwndOtherClient, HWND hwndSubMenu,
                                    INT idMenuItem );
static USHORT GetDefaultId        ( HWND hwndClient, USHORT idSubMenu );
static VOID   ChangeSelType       ( HWND hwndClient, USHORT idMenuitem );
static VOID   ChangeBackground    ( HWND hwndClient, USHORT idMenuitem );
static VOID   ToggleMiniIcons     ( HWND hwndClient );

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*-------------------------- CtxtmenuCreate --------------------------*/
/*                                                                    */
/*  CREATE THE CONTEXT MENU.                                          */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         pointer to CNRITEM that mouse pointer is over              */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID CtxtmenuCreate( HWND hwndClient, PCNRITEM pciSelected )
{
    POINTL    ptl;
    BOOL      fSuccess = TRUE;
    USHORT    idStart = IDM_VIEW_SUBMENU;
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    HWND      hwndMenu = WinLoadMenu( hwndClient, 0, ID_CONTEXT_MENU );
    PINSTANCE pi = INSTDATA( hwndClient );

    if( !pi )
    {
        Msg( "CtxtmenuCreate cant get Inst data. RC(%X)", HWNDERR(hwndClient) );

        return;
    }

    if( !hwndMenu )
    {
        Msg( "CtxtmenuCreate WinLoadMenu RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Save the pointer to the CNRITEM that was under the mouse pointer when
    // the context menu was invoked. We will need this value when we get
    // WM_COMMAND values because the WM_COMMAND message doesn't tell you which
    // container record the context menu applies to. If the pointer is over
    // white space when the context menu is invoked, pciSelected will be NULL

    pi->pciSelected = pciSelected;

    // Selected records are tagged with their CNRITEM.fSelected flag. Initialize
    // all container records to 'not selected'.

    TurnOffSelFlags( hwndCnr );

    // Turn on source emphasis for applicable records. The function that does
    // this also checks if any of the records represent directories. Init this
    // bool to FALSE. It will be set to TRUE if a directory is found.

    pi->fDirSelected = FALSE;

    TurnOnSourceEmphasis( hwndClient );

    // Get the position of the mouse pointer

    fSuccess = WinQueryPointerPos( HWND_DESKTOP, &ptl );

    if( fSuccess )
    {
        // Convert the position of the mouse pointer to coordinates with respect
        // to the client window.

        fSuccess = WinMapWindowPoints( HWND_DESKTOP, hwndClient, &ptl, 1 );

        if( !fSuccess )
            Msg( "CtxtmenuCreate WinMapWindowPoints failed! RC(%X)",
                 HWNDERR( hwndClient ) );
    }
    else
        Msg( "CtxtmenuCreate WinQueryPointerPos failed! RC(%X)",
             HWNDERR( hwndClient ) );

    // Add/remove menu items, etc.

    TailorMenu( hwndCnr, hwndMenu, pciSelected );

    if( !WinPopupMenu( hwndClient, hwndClient, hwndMenu, ptl.x, ptl.y,
                       idStart, PU_HCONSTRAIN | PU_VCONSTRAIN | PU_KEYBOARD |
                       PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 | PU_NONE ) )
        Msg( "CtxtmenuCreate WinPopupMenu failed! RC(%X)", HWNDERR(hwndClient));
}

/**********************************************************************/
/*-------------------------- CtxtmenuCommand -------------------------*/
/*                                                                    */
/*  PROCESS A WM_COMMAND MESSAGE FROM THE CONTEXT MENU.               */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         command id,                                                */
/*         command source                                             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID CtxtmenuCommand( HWND hwndClient, ULONG idCommand, ULONG ulCmdSrc )
{
    switch( idCommand )
    {
        case IDM_VIEW_TREEICON:

            CtxtmenuSetView( hwndClient, CV_TREE | CV_ICON );

            break;

        case IDM_VIEW_TREENAME:

            CtxtmenuSetView( hwndClient, CV_TREE | CV_NAME );

            break;

        case IDM_VIEW_TREETEXT:

            CtxtmenuSetView( hwndClient, CV_TREE | CV_TEXT );

            break;

        case IDM_VIEW_NAME:

            CtxtmenuSetView( hwndClient, CV_NAME );

            break;

        case IDM_VIEW_NAMEFLOWED:

            CtxtmenuSetView( hwndClient, CV_NAME | CV_FLOW );

            break;

        case IDM_VIEW_TEXT:

            CtxtmenuSetView( hwndClient, CV_TEXT );

            break;

        case IDM_VIEW_TEXTFLOWED:

            CtxtmenuSetView( hwndClient, CV_TEXT | CV_FLOW );

            break;

        case IDM_VIEW_ICON:

            CtxtmenuSetView( hwndClient, CV_ICON );

            break;

        case IDM_VIEW_DETAILS:

            CtxtmenuSetView( hwndClient, CV_DETAIL );

            break;

        case IDM_VIEW_MINI:

            ToggleMiniIcons( hwndClient );

            break;

        case IDM_CREATE_NEWWIN:

            // Only go thru all selected records if this command came from the
            // context menu (this WM_COMMAND message is also sent by the client
            // window on a CN_ENTER).

            NewWindows( hwndClient, ulCmdSrc == CMDSRC_MENU ? TRUE : FALSE );

            break;

        case IDM_ARRANGE:

            if( !WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY, CM_ARRANGE,
                                    NULL, NULL ) )
                Msg( "CtxtmenuCommand CM_ARRANGE RC(%X)", HWNDERR(hwndClient) );

            break;

        case IDM_SEL_SINGLE:
        case IDM_SEL_MULTIPLE:
        case IDM_SEL_EXTENDED:

            ChangeSelType( hwndClient, idCommand );

            break;

        case IDM_BKGD_RED:
        case IDM_BKGD_WHITE:
        case IDM_BKGD_YELLOW:
        case IDM_BKGD_BLUE:
        case IDM_BKGD_GREY:
        case IDM_BKGD_BITMAP:

            ChangeBackground( hwndClient, idCommand );

            break;

        case IDM_SORT_NAME:
        case IDM_SORT_DATETIME:
        case IDM_SORT_DIRORDER:

            // In sort.c

            SortContainer( hwndClient, idCommand );

            break;

        case IDM_VIEW_SUBMENU:
        case IDM_SORT_SUBMENU:
        {
            // Find out the default id of the submenu and simulate that menu
            // item being pressed.

            USHORT id = GetDefaultId( hwndClient, idCommand );

            if( id )
                WinSendMsg( hwndClient, WM_COMMAND, MPFROM2SHORT( id, 0 ),
                            MPFROM2SHORT( CMDSRC_MENU, 0 ) );

            break;
        }

        default:
        {
            PINSTANCE pi = INSTDATA( hwndClient );

            // hwndFrame was set when the item was put into the menu. It is the
            // frame window handle that contains the directory pointed to by
            // the text in the menuitem.

            if( pi && idCommand >= IDM_OTHERWIN_ITEM1 &&
                idCommand <= IDM_OTHERWIN_LASTITEM )
                WinSetFocus( HWND_DESKTOP,
                             pi->hwndFrame[ idCommand - IDM_OTHERWIN_ITEM1 ] );

            break;
        }
    }
}

/**********************************************************************/
/*-------------------------- CtxtmenuSetView -------------------------*/
/*                                                                    */
/*  SET THE TYPE OF VIEW FOR THE CONTAINER                            */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         view type to set to                                        */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID CtxtmenuSetView( HWND hwndClient, ULONG ulViewType )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    CNRINFO   cnri;

    if( !pi )
    {
        Msg( "CtxtmenuSetView cant get Inst data. RC(%X)", HWNDERR(hwndClient));

        return;
    }

    cnri.cb = sizeof( CNRINFO );

    // Set the container window attributes: Set the container view mode. Use a
    // container title. Put a separator between the title and the records
    // beneath it. Make the container title read-only. Make it so the container
    // sends itself a CM_PAINTBACKGROUND message so we can paint the container
    // background ourselves in its subclassed window procedure. Add Mixed
    // Target emphasis so the place where records will be dropped is evident.

    cnri.flWindowAttr = ulViewType | CA_CONTAINERTITLE | CA_TITLESEPARATOR |
                        CA_MIXEDTARGETEMPH | CA_TITLEREADONLY |
                        CA_OWNERPAINTBACKGROUND;

    SetViewType( ulViewType, pi );

    // If we are in DETAIL view, tell the container that we will be
    // using column headings.

    if( ulViewType & CV_DETAIL )
        cnri.flWindowAttr |= CA_DETAILSVIEWTITLES;


    if( ulViewType & CV_TREE )
    {
        // Use default spacing between levels in the tree view. Also use
        // the default width of a line that shows record relationships.

        cnri.cxTreeIndent = -1;
        cnri.cxTreeLine   = -1;

        cnri.flWindowAttr |=  CA_TREELINE;
    }

    (void) strcat( pi->szCnrTitle, pi->szDirectory );

    cnri.pszCnrTitle = pi->szCnrTitle;

    // Set the line spacing between rows to be the minimal value so we conserve
    // on container whitespace.

    cnri.cyLineSpacing = 0;

    // Produce mini icons if that is currently in effect. CV_MINI is not
    // currently working (11/30/92) so we use slBitmapOrIcon as well as set the
    // CMA_SLBITMAPORICON bit in the CM_SETCNRINFO message to get the mini
    // icons. This method was recommended by Peter Haggar, one of the
    // container control developers, on Compuserve. The mini icons are
    // supposed to be the height of the action bar. We set the CV_MINI bit,
    // but as of this writing that really doesn't do anything.

    if( pi->fMiniIcons )
    {
        cnri.flWindowAttr |= CV_MINI;
        cnri.slBitmapOrIcon.cx = WinQuerySysValue( HWND_DESKTOP, SV_CYMENU );
        cnri.slBitmapOrIcon.cy = cnri.slBitmapOrIcon.cx;
    }
    else
        cnri.slBitmapOrIcon.cx = cnri.slBitmapOrIcon.cy = 0;

    // Set the container parameters. Note that mp2 specifies which fields of
    // of the CNRINFO structure to use. The CMA_FLWINDOWATTR says to use
    // flWindowAttr to specify which 'Window Attribute' fields to use.

    if( !WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY, CM_SETCNRINFO,
                            MPFROMP( &cnri ),
                            MPFROMLONG( CMA_FLWINDOWATTR | CMA_CNRTITLE |
                                      CMA_LINESPACING | CMA_SLBITMAPORICON ) ) )
        Msg( "CtxtmenuSetView CM_SETCNRINFO RC(%X)", HWNDERR( hwndClient ) );

    // The CM_ARRANGE message is applicable only in ICON view. It will arrange
    // the icons according to CUA. Note that this message is unnecessary if
    // the CCS_AUTOPOSITION style is used on the WinCreateWindow call for the
    // container. The problem with using that style is that you have no control
    // over *when* the arranging is done.

    if( ulViewType == CV_ICON )
        if( !WinSendDlgItemMsg( hwndClient, CNR_DIRECTORY, CM_ARRANGE, NULL,
                                NULL ) )
            Msg( "CtxtmenuSetView CM_ARRANGE RC(%X)", HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*--------------------------- SetViewType ----------------------------*/
/*                                                                    */
/*  STORE INFORMATION SENSITIVE TO VIEW TYPE                          */
/*                                                                    */
/*  INPUT: view being set,                                            */
/*         pointer to client's instance data                          */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID SetViewType( ULONG ulViewType, PINSTANCE pi )
{
    switch( ulViewType )
    {
        case (CV_TREE | CV_ICON):

            (void) strcpy( pi->szCnrTitle, "Tree/Icon view - " );

            pi->flCurrentView = CV_TREE;

            break;

        case (CV_TREE | CV_TEXT):

            (void) strcpy( pi->szCnrTitle, "Tree/Text view - " );

            pi->flCurrentView = CV_TREE;

            break;

        case (CV_TREE | CV_NAME):

            (void) strcpy( pi->szCnrTitle, "Tree/Name view - " );

            pi->flCurrentView = CV_TREE;

            break;

        case CV_ICON:

            (void) strcpy( pi->szCnrTitle, "Icon view - " );

            pi->flCurrentView = CV_ICON;

            break;

        case CV_NAME:

            (void) strcpy( pi->szCnrTitle, "Name view - " );

            pi->flCurrentView = CV_NAME;

            break;

        case (CV_NAME | CV_FLOW):

            (void) strcpy( pi->szCnrTitle, "Name/flowed view - " );

            pi->flCurrentView = CV_NAME;

            break;

        case CV_DETAIL:

            (void) strcpy( pi->szCnrTitle, "Detail view - " );

            pi->flCurrentView = CV_DETAIL;

            break;

        case CV_TEXT:

            (void) strcpy( pi->szCnrTitle, "Text view - " );

            pi->flCurrentView = CV_TEXT;

            break;

        case (CV_TEXT | CV_FLOW):

            (void) strcpy( pi->szCnrTitle, "Text/flowed view - " );

            pi->flCurrentView = CV_TEXT;

            break;
    }
}

/**********************************************************************/
/*---------------------------- CtxtmenuEnd ---------------------------*/
/*                                                                    */
/*  PROCESS WM_MENUEND MESSAGE FOR THE CONTEXT MENU.                  */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID CtxtmenuEnd( HWND hwndClient )
{
    PINSTANCE pi = INSTDATA( hwndClient );

    if( !pi )
    {
        Msg( "CtxtmenuEnd cant get Instdata RC(%X)", HWNDERR(hwndClient));

        return;
    }

    TurnOffSourceEmphasis( hwndClient );

    // Reset variable that was only used during context menu processing

    pi->fDirSelected = FALSE;
}

/**********************************************************************/
/*-------------------------- TurnOffSelFlags -------------------------*/
/*                                                                    */
/*  TURN OFF THE fSelected FLAG FOR ALL CONTAINER RECORDS.            */
/*                                                                    */
/*  INPUT: container window handle                                    */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID TurnOffSelFlags( HWND hwndCnr )
{
    PCNRITEM pci = NULL;
    USHORT usWhatRec = CMA_FIRST;

    for( ; ; )
    {
        pci = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORD, MPFROMP( pci ),
                                     MPFROM2SHORT( usWhatRec, CMA_ITEMORDER ) );

        if( (INT) pci == -1 )
        {
            Msg( "TurnOffSelFlags CM_QUERYRECORD RC(%X)", HWNDERR( hwndCnr ) );

            break;
        }

        if( !pci )
            break;

        pci->fSelected = FALSE;

        usWhatRec = CMA_NEXT;
    }
}

/**********************************************************************/
/*----------------------------- NewWindows ---------------------------*/
/*                                                                    */
/*  CREATE NEW WINDOWS FOR SELECTED CONTAINER RECORDS.                */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         flag indicating if all selected records are to be looked at*/
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID NewWindows( HWND hwndClient, BOOL fAllSelected )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    PCNRITEM  pci = NULL;
    USHORT    usWhatRec = CMA_FIRST;

    if( !pi )
    {
        Msg( "NewWindows cant get Inst data RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Create new window for the currently selected record

    NewWin( hwndCnr, pi->szDirectory, pi->pciSelected );

    // Create new windows for all records that have source emphasis if the
    // flag passed to this function indicates that we are to do this. We would
    // not want to do this if the user double-clicked on just one record vs.
    // selecting 'CreateNewWindow' from the menu.

    for( ; fAllSelected ; )
    {
        pci = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORD, MPFROMP( pci ),
                                     MPFROM2SHORT( usWhatRec, CMA_ITEMORDER ) );

        if( (INT) pci == -1 )
        {
            Msg( "NewWindows CM_QUERYRECORD RC(%X)", HWNDERR( hwndCnr ) );

            break;
        }

        if( !pci )
            break;

        if( pci->fSelected && pci != pi->pciSelected )
            NewWin( hwndCnr, pi->szDirectory, pci );

        usWhatRec = CMA_NEXT;
    }
}

/**********************************************************************/
/*------------------------ TurnOnSourceEmphasis ----------------------*/
/*                                                                    */
/*  TURN ON SOURCE EMPHASIS FOR APPLICABLE CONTAINER RECORDS.         */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID TurnOnSourceEmphasis( HWND hwndClient )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    BOOL      fRecsSelected = TRUE;
    INT       iSelCount;

    if( !pi )
    {
        Msg( "TurnOn..Emphasis cant get Inst data RC(%X)", HWNDERR(hwndClient));

        return;
    }

    // NOTE: CRA_SOURCE is defined in *our* header file because CRA_SOURCE
    // appeared in the 10/30/92 Service Pack and is not yet in the toolkit
    // header files at this writing.

    // If no record is selected (pciSelected is NULL), we set source emphasis
    // to the container itself because the mouse pointer is over whitespace.

    if( !pi->pciSelected )
    {
        if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP( NULL ),
                         MPFROM2SHORT( TRUE, CRA_SOURCE ) ) )
            Msg( "CM_SETRECORDEMPHASIS failed! RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Get the number of records that are currently in the 'selected' state in
    // the container. We only care about *all* selected records if the one that
    // is now under the mouse pointer is also in the selected state. If it
    // is not, CountSelectedRecs will pass us back a -1 to indicate that we only
    // need to process that record even though others are in selected state.
    // Also keep track in pi->fDirSelected if there are any records that
    // represent directories. This will become useful during menu tailoring.
    // Last thing: set the fSelected BOOL in the CNRITEM struct so we know
    // which records we qualified for source emphasis. When we get the
    // WM_MENUEND message we turn off source emphasis. The WM_MENUEND message
    // happens before we get the WM_COMMAND message so it is too late when we
    // get the WM_COMMAND message to just query which records have the source
    // emphasis to figure out which recs to process.

    iSelCount = CountSelectedRecs( hwndCnr, pi->pciSelected );

    if( iSelCount == -1 )
    {
        fRecsSelected = FALSE;

        iSelCount = 1;
    }

    if( iSelCount )
    {
        if( fRecsSelected )
        {
            PCNRITEM pci;
            INT      i;

            pci = (PCNRITEM) CMA_FIRST;

            // For each selected record turn on source emphasis

            for( i = 0; i < iSelCount; i++ )
            {
                pci = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORDEMPHASIS,
                                             MPFROMP( pci ),
                                             MPFROMSHORT( CRA_SELECTED ) );

                if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP( pci ),
                                 MPFROM2SHORT( TRUE, CRA_SOURCE ) ) )
                    Msg( "CM_SETRECORDEMPHASIS failed! RC(%X)",
                         HWNDERR( hwndClient ) );

                pci->fSelected = TRUE;

                if( (pci->attrFile & FILE_DIRECTORY) &&
                     pci->szFileName[0] != '.' )
                    pi->fDirSelected = TRUE;
            }
        }
        else
        {
            if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS,
                             MPFROMP( pi->pciSelected ),
                             MPFROM2SHORT( TRUE, CRA_SOURCE ) ) )
                Msg( "CM_SETRECORDEMPHASIS failed! RC(%X)",HWNDERR(hwndClient));

            pi->pciSelected->fSelected = TRUE;

            if( (pi->pciSelected->attrFile & FILE_DIRECTORY) &&
                 pi->pciSelected->szFileName[0] != '.' )
                pi->fDirSelected = TRUE;
        }
    }
}

/**********************************************************************/
/*----------------------- TurnOffSourceEmphasis ----------------------*/
/*                                                                    */
/*  TURN OFF SOURCE EMPHASIS FOR THOSE RECORDS THAT HAVE IT.          */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID TurnOffSourceEmphasis( HWND hwndClient )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );

    if( !pi )
    {
        Msg( "TurnOff..Emphasis cant get Instdata RC(%X)", HWNDERR(hwndClient));

        return;
    }

    // If no record is selected (pciSelected is NULL), we had set source
    // emphasis to the container itself because the mouse pointer was over
    // whitespace. So turn it off for the container. Otherwise, turn it off
    // for the selected record and all others that have source emphasis

    if( pi->pciSelected )
    {
        PCNRITEM pci = (PCNRITEM) CMA_FIRST;

        if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS,
                         MPFROMP( pi->pciSelected ),
                         MPFROM2SHORT( FALSE, CRA_SOURCE ) ) )
            Msg( "TurnOff..Emphasis CM_SETRECORDEMPHASIS failed! RC(%X)",
                 HWNDERR( hwndClient ) );

        for( ; ; )
        {
            pci = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORDEMPHASIS,
                                     MPFROMP( pci ), MPFROMSHORT( CRA_SOURCE) );

            if( !pci )
                break;

            if( pci != pi->pciSelected )
                if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP( pci ),
                                 MPFROM2SHORT( FALSE, CRA_SOURCE ) ) )
                    Msg( "TurnOff... CM_SETRECORDEMPHASIS failed! RC(%X)",
                         HWNDERR( hwndClient ) );
        }
    }
    else
        if( !WinSendMsg( hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP( NULL ),
                         MPFROM2SHORT( FALSE, CRA_SOURCE ) ) )
            Msg( "TurnOff...Emphasis CM_SETRECORDEMPHASIS failed! RC(%X)",
                 HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*------------------------ CountSelectedRecs -------------------------*/
/*                                                                    */
/*  COUNT THE NUMBER OF RECORDS THAT ARE CURRENTLY SELECTED.          */
/*                                                                    */
/*  INPUT: pointer to the record that was under the pointer.          */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: number of selected recs, 0 if none, -1 if the record that */
/*          the mouse pointer was under was not selected.             */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static INT CountSelectedRecs( HWND hwndCnr, PCNRITEM pciUnderMouse )
{
    INT      iCount = 0;
    PCNRITEM pci;
    BOOL     fFound = FALSE;

    if( pciUnderMouse )
    {
        pci = (PCNRITEM) CMA_FIRST;

        while( pci )
        {
            pci = (PCNRITEM) WinSendMsg( hwndCnr, CM_QUERYRECORDEMPHASIS,
                                         MPFROMP( pci ),
                                         MPFROMSHORT( CRA_SELECTED ) );
            if( pci )
            {
                if( pci == pciUnderMouse )
                    fFound = TRUE;

                iCount++;
            }
        }

        if( !fFound )
            iCount = -1;
    }

    return iCount;
}

/**********************************************************************/
/*------------------------------ NewWin ------------------------------*/
/*                                                                    */
/*  CREATE A NEW WINDOW FOR A DIRECTORY.                              */
/*                                                                    */
/*  INPUT: container window handle,                                   */
/*         pointer to base directory name for this client window,     */
/*         pointer to CNRITEM record to create window for             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID NewWin( HWND hwndCnr, PSZ szBaseDir, PCNRITEM pci )
{
    // If the user selected a directory, create another directory window for it

    if( pci && (pci->attrFile & FILE_DIRECTORY) && pci->szFileName[0] != '.' )
    {
        CHAR szDirectory[ CCHMAXPATH + 1 ];

        (void) strcpy( szDirectory, szBaseDir );

        // Recursively go up the tree and add the subdirectory names to the
        // fully qualified directory name.

        FullyQualify( szDirectory, hwndCnr, pci );

        // CreateDirectoryWin is in CREATE.C. By specifying pci as the third
        // parameter we are saying that we want the new container to share
        // records with this one.

        (void) CreateDirectoryWin( szDirectory, hwndCnr, pci );
    }
}

/**********************************************************************/
/*--------------------------- TailorMenu -----------------------------*/
/*                                                                    */
/*  TAILOR THE MENU FOR THE SELECTED OBJECT                           */
/*                                                                    */
/*  INPUT: container window handle,                                   */
/*         menu window handle,                                        */
/*         pointer to CNRITEM record that is selected                 */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID TailorMenu( HWND hwndCnr, HWND hwndMenu, PCNRITEM pciSelected )
{
    CNRINFO   cnri;
    PINSTANCE pi = INSTDATA( PARENT( hwndCnr ) );

    if( !pi )
    {
        Msg( "TailorMenu cant get Instdata RC(%X)", HWNDERR( hwndCnr ) );

        return;
    }

    // Set the MS_CONDITIONALCASCADE bit and default menu item for submenus.

    SetConditionalCascade( hwndMenu, IDM_VIEW_SUBMENU, IDM_VIEW_ICON );
    SetConditionalCascade( hwndMenu, IDM_SORT_SUBMENU, IDM_SORT_DIRORDER );

    // Take the CreateNewWindow menu item off the menu if the mouse pointer is
    // not over a container record or if that record is not a directory or if
    // no selected records are directories. We can only create a new directory
    // window for a directory record.

    if( !pciSelected || !(pi->fDirSelected ||
        ((pciSelected->attrFile & FILE_DIRECTORY)
          && pciSelected->szFileName[0] != '.')) )
        if( !WinSendMsg( hwndMenu, MM_DELETEITEM,
                         MPFROM2SHORT( IDM_CREATE_NEWWIN, FALSE ), NULL ) )
            Msg( "TailorMenu MM_DELETEITEM failed for IDM_CREATE_NEWWIN RC(%X)",
                  HWNDERR( hwndMenu ) );

    if( WinSendMsg( hwndCnr, CM_QUERYCNRINFO, MPFROMP( &cnri ),
                    MPFROMLONG( sizeof( CNRINFO ) ) ) )
    {
        // The Arrange menu item is only applicable to ICON VIEW. Since the
        // Tree view can be or'ed with CV_ICON, we need to first check if we
        // are in Tree view before we care about CV_ICON by itself

        if( (cnri.flWindowAttr & CV_TREE) || !(cnri.flWindowAttr & CV_ICON) )
            if( !WinSendMsg( hwndMenu, MM_DELETEITEM,
                             MPFROM2SHORT( IDM_ARRANGE, FALSE ), NULL ) )
                Msg( "TailorMenu MM_DELETEITEM failed for IDM_ARRANGE RC(%X)",
                     HWNDERR( hwndMenu ) );
    }
    else
        Msg( "TailorMenu MM_DELETEITEM failed for IDM_CREATE_NEWWIN RC(%X)",
              HWNDERR( hwndCnr ) );

    // Add a menu item for each directory window (other than our's) that we
    // find on the desktop. If there aren't any others, delete the OtherWindow
    // Submenu since it wouldn't apply

    if( !AddOtherWindows( hwndCnr, hwndMenu ) )
        if( !WinSendMsg( hwndMenu, MM_DELETEITEM,
                         MPFROM2SHORT( IDM_OTHERWIN_SUBMENU, FALSE ), NULL ) )
            Msg( "TailorMenu MM_DELETEITEM failed for IDM_OTHERWIN_SUB RC(%X)",
                 HWNDERR( hwndMenu ) );

    // If mini icons are in effect, show that by checking the menu item

    if( pi->fMiniIcons )
        if( !WinSendMsg( hwndMenu, MM_SETITEMATTR,
                         MPFROM2SHORT( IDM_VIEW_MINI, TRUE ),
                         MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED ) ) )
            Msg( "TailorMenu MM_SETITEMATTR RC(%X)", HWNDERR( hwndMenu ) );
}

/**********************************************************************/
/*---------------------- SetConditionalCascade -----------------------*/
/*                                                                    */
/*  SET A SUBMENU TO BE A CONDITIONAL CASCADE SUBMENU                 */
/*                                                                    */
/*  INPUT: menu window handle,                                        */
/*         id of submenu to make conditionally cascaded,              */
/*         id of item to make the default item of the cascaded menu   */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID SetConditionalCascade( HWND hwndMenu, USHORT idSubMenu,
                                   USHORT idDefaultItem )
{
    MENUITEM mi;

    if( WinSendMsg( hwndMenu, MM_QUERYITEM,
                    MPFROM2SHORT( idSubMenu, TRUE ), &mi ) )
    {
        // Set the MS_CONDITIONALCASCADE bit for the submenu.

        if( WinSetWindowBits( mi.hwndSubMenu, QWL_STYLE, MS_CONDITIONALCASCADE,
                              MS_CONDITIONALCASCADE ) )
        {
            // Set cascade menu default

            if( !WinSendMsg( mi.hwndSubMenu, MM_SETDEFAULTITEMID,
                             MPFROMSHORT( idDefaultItem ), NULL ) )
               Msg( "MM_SETDEFAULTITEMID failed! RC(%X)", HWNDERR( hwndMenu ) );
        }
        else
            Msg( "Set...Cascade WinSetWindowBits RC(%X)", HWNDERR( hwndMenu ) );
    }
    else
        Msg( "Set...Cascade MM_QUERYITEM failed! RC(%X)", HWNDERR( hwndMenu ) );
}

/**********************************************************************/
/*------------------------- AddOtherWindows --------------------------*/
/*                                                                    */
/*  ADD OTHER DIRECTORY WINDOWS AS ITEMS ON THE "OTHER WINDOW" SUBMENU*/
/*                                                                    */
/*  INPUT: container window handle,                                   */
/*         menu window handle                                         */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: number of other windows                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static INT AddOtherWindows( HWND hwndCnr, HWND hwndMenu )
{
    INT       iOthers = 0, idMenuItem = IDM_OTHERWIN_ITEM1;
    HWND      hwndEnum;
    MENUITEM  miSubMenu;

    if( !WinSendMsg( hwndMenu, MM_QUERYITEM,
                     MPFROM2SHORT( IDM_OTHERWIN_SUBMENU, TRUE ), &miSubMenu ) )
    {
        Msg( "AddOtherWindows MM_QUERYITEM RC(%X)", HWNDERR( hwndMenu ) );

        return 0;
    }

    hwndEnum = WinBeginEnumWindows( HWND_DESKTOP );

    if( hwndEnum )
    {
        HWND hwndFrame, hwndClient;
        CHAR szClass[ 50 ];

        for( ; ; )
        {
            hwndFrame = WinGetNextWindow( hwndEnum );

            if( !hwndFrame )
                break;

            // If we found a frame window (child of the desktop), check to see
            // if it has a client window because that's the one that would have
            // the class that we're looking for

            hwndClient = WinWindowFromID( hwndFrame, FID_CLIENT );

            if( hwndClient )
            {
                // Make sure we are checking NULL-terminated strings

                (void) memset( szClass, 0, sizeof( szClass ) );

                // Get the class of this client window. If it is one of ours
                // and it isn't the current window, add it to the OtherWindow
                // submenu.

                if( hwndClient != PARENT( hwndCnr ) &&
                    WinQueryClassName( hwndClient, sizeof(szClass), szClass ) )
                {
                    if( !strcmp( szClass, DIRECTORY_WINCLASS ) )
                        if( AddOtherWinItem( PARENT( hwndCnr ), hwndFrame,
                               hwndClient, miSubMenu.hwndSubMenu, idMenuItem ) )
                        {
                            idMenuItem++;

                            iOthers++;
                        }
                }
            }
        }

        WinEndEnumWindows( hwndEnum );
    }
    else
        Msg( "AddOtherWindows WinBeginEnumWindows RC(%X)", HWNDERR( hwndCnr ) );

    return iOthers;
}

/**********************************************************************/
/*------------------------- AddOtherWinItem --------------------------*/
/*                                                                    */
/*  ADD THE OTHER DIRECTORY WINDOW AS A MENU ITEM.                    */
/*                                                                    */
/*  INPUT: client window handle of our window,                        */
/*         frame window handle of other window,                       */
/*         client window handle of other window,                      */
/*         OtherWindow submenu window handle,                         */
/*         menu item id                                               */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if successful or not                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL AddOtherWinItem( HWND hwndClient, HWND hwndOtherFrame,
                             HWND hwndOtherClient, HWND hwndSubMenu,
                             INT idMenuItem )
{
    PINSTANCE piUs = INSTDATA( hwndClient );
    PINSTANCE piOther = INSTDATA( hwndOtherClient );
    BOOL      fSuccess = TRUE;

    if( piUs && piOther )
    {
        MENUITEM  miItem;
        SHORT     sMenuRC;

        (void) memset( &miItem, 0, sizeof( MENUITEM ) );

        miItem.iPosition = MIT_END;
        miItem.afStyle   = MIS_TEXT;
        miItem.id        = idMenuItem;

        // Save the frame window handle so we can easily retrieve it if this
        // menu item is selected so we can change focus to that frame window.

        piUs->hwndFrame[ idMenuItem - IDM_OTHERWIN_ITEM1 ] = hwndOtherFrame;

        sMenuRC = (SHORT) WinSendMsg( hwndSubMenu, MM_INSERTITEM,
                                      MPFROMP( &miItem ),
                                      MPFROMP( piOther->szDirectory ) );

        if( sMenuRC == MIT_MEMERROR || sMenuRC == MIT_ERROR )
        {
            fSuccess = FALSE;

            Msg( "AddOtherWinItem MM_INSERTITEM MenuRC(%d) RC(%X)",
                 sMenuRC, HWNDERR( hwndClient ) );
        }
    }
    else
    {
        fSuccess = FALSE;

        Msg( "AddOtherWindows cant get Inst data RC(%X)", HWNDERR(hwndClient) );
    }

    return fSuccess;
}

/**********************************************************************/
/*-------------------------- GetDefaultId ----------------------------*/
/*                                                                    */
/*  GET THE DEFAULT ID FOR A MS_CONDITIONALCASCADE SUBMENU            */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         id of submenu                                              */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: id of default item                                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static USHORT GetDefaultId( HWND hwndClient, USHORT idSubMenu )
{
    USHORT  id = 0;
    HWND    hwndMenu = WinWindowFromID( hwndClient, ID_CONTEXT_MENU );

    if( hwndMenu )
    {
        MENUITEM mi;

        if( WinSendMsg( hwndMenu, MM_QUERYITEM, MPFROM2SHORT( idSubMenu, TRUE ),
                        &mi ))
        {
            id = (USHORT) WinSendMsg( mi.hwndSubMenu,
                                      MM_QUERYDEFAULTITEMID, NULL, NULL );

            if( !id )
                Msg( "GetDefaultId MM_QUERYDEFAULTITEMID RC(%X)",
                      HWNDERR( hwndClient ) );
        }
        else
            Msg( "GetDefaultId MM_QUERYITEM RC(%X)", HWNDERR( hwndClient ) );
    }

/*****************************************************************************
BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG

    else
        Msg( "GetDefaultId WinWindowFromID RC(%X)", HWNDERR( hwndClient ) );

For some reason, only in a specific instance, the WinWindowFromID fails but no
valid error code is returned from WinGetLastError. This happens under the
following circumstances:

 - the popup menu is active
 - you select the Other Window submenu but don't select a menu item
 - you select a conditionally cascaded submenu but not the cascade button

I tried to nail this down but could not.

BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG BUG
******************************************************************************/

    return id;
}

/**********************************************************************/
/*-------------------------- ChangeSelType ---------------------------*/
/*                                                                    */
/*  CHANGE THE CONTAINER SELECTION TYPE                               */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         id of menu item that determines selection type             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID ChangeSelType( HWND hwndClient, USHORT idMenuitem )
{
    HWND  hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    ULONG flSelType = CCS_SINGLESEL | CCS_MULTIPLESEL | CCS_EXTENDSEL;

    // Clear existing selection type

    if( !WinSetWindowBits( hwndCnr, QWL_STYLE, 0, flSelType ) )
        Msg( "ChangeSelType WinSetWindowBits RC(%X)", HWNDERR( hwndClient ) );

    switch( idMenuitem )
    {
        case IDM_SEL_SINGLE:

            flSelType = CCS_SINGLESEL;

            break;

        case IDM_SEL_MULTIPLE:

            flSelType = CCS_MULTIPLESEL;

            break;

        case IDM_SEL_EXTENDED:

            flSelType = CCS_EXTENDSEL;

            break;
    }

    // Set new selection type

    if( !WinSetWindowBits( hwndCnr, QWL_STYLE, flSelType, flSelType ) )
        Msg( "ChangeSelType WinSetWindowBits RC(%X)", HWNDERR( hwndClient ) );
}

/**********************************************************************/
/*------------------------- ChangeBackground -------------------------*/
/*                                                                    */
/*  CHANGE THE BACKGROUND COLOR OF THE CONTAINER.                     */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         id of menu item that determines selection type             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID ChangeBackground( HWND hwndClient, USHORT idMenuitem )
{
    PINSTANCE pi = INSTDATA( hwndClient );

    if( !pi )
    {
        Msg( "ChangeBackground cant get Instdata RC(%X)", HWNDERR(hwndClient) );

        return;
    }

    switch( idMenuitem )
    {
        case IDM_BKGD_RED:

            pi->clrBackground = CLR_RED;

            break;

        case IDM_BKGD_WHITE:

            pi->clrBackground = CLR_WHITE;

            break;

        case IDM_BKGD_YELLOW:

            pi->clrBackground = CLR_YELLOW;

            break;

        case IDM_BKGD_BLUE:

            pi->clrBackground = CLR_BLUE;

            break;

        case IDM_BKGD_GREY:

            pi->clrBackground = CLR_PALEGRAY;

            break;

        case IDM_BKGD_BITMAP:

            pi->clrBackground = 0;

            break;
    }

    // Note that you must use TRUE as the last parameter in WinInvalidateRect
    // to tell the container to paint its child windows. The container is
    // made up of more than one window, many in the case of Details view.

    if( !WinInvalidateRect( WinWindowFromID( hwndClient, CNR_DIRECTORY ), NULL,
                            TRUE ) )
        Msg( "ChangeBackground WinInvalidateRect RC(%X)", HWNDERR(hwndClient) );
}

/**********************************************************************/
/*------------------------- ToggleMiniIcons --------------------------*/
/*                                                                    */
/*  TOGGLE ON OR OFF MINI ICONS                                       */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID ToggleMiniIcons( HWND hwndClient )
{
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );
    PINSTANCE pi = INSTDATA( hwndClient );
    CNRINFO   cnri;

    if( !pi )
    {
        Msg( "ToggleMiniIcons cant get Instdata RC(%X)", HWNDERR( hwndCnr ) );

        return;
    }

    // Toggle the BOOL that lets the rest of the program know what mode we are
    // in.

    pi->fMiniIcons ^= TRUE;

    // It is necessary to use slBitmapOrIcon instead of CV_MINI because
    // CV_MINI does not work as of 11/30/92

    if( pi->fMiniIcons )
    {
        cnri.slBitmapOrIcon.cx = WinQuerySysValue( HWND_DESKTOP, SV_CYMENU );
        cnri.slBitmapOrIcon.cy = cnri.slBitmapOrIcon.cx;
    }
    else
        cnri.slBitmapOrIcon.cx = cnri.slBitmapOrIcon.cy = 0;

    if( !WinSendMsg( hwndCnr, CM_SETCNRINFO, MPFROMP( &cnri ),
                     MPFROMLONG( CMA_SLBITMAPORICON ) ) )
        Msg( "ToggleMiniIcons CM_SETCNRINFO RC(%X)", HWNDERR( hwndCnr ) );

    if( !WinSendMsg( hwndCnr, CM_INVALIDATERECORD, NULL, NULL ) )
        Msg( "ToggleMiniIcons CM_INVALIDATERECORD RC(%X)", HWNDERR( hwndCnr ) );
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
