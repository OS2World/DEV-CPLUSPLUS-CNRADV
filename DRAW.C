/*********************************************************************
 *----------------------------- DRAG.C ------------------------------*
 *                                                                   *
 * MODULE NAME :  draw.c                 AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  12-05-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It contains the functions     *
 *  necessary to implement ownerdraw columns and background.         *
 *                                                                   *
 * NOTE: We use CA_OWNERPAINTBACKGROUND to cause the container to    *
 *       send itself a CM_PAINTBACKGROUND message. We then subclass  *
 *       the container to catch this message and paint the container *
 *       as requested the last time the user selected an item from   *
 *       the Background submenu. We do this for selected colors as   *
 *       well as for a Bitmap. If you simply want to set the back-   *
 *       ground color of the container, WinSetPresParam would be the *
 *       way to go. I mainly did it this way to demonstrate the      *
 *       CA_OWNERPAINTBACKGROUND method. Also this is the only way   *
 *       you could paint a bitmap on the background.                 *
 *                                                                   *
 * IMPORTANT NOTE: While coding this module I found out that when    *
 *                 a container item is selected or unselected, that  *
 *                 causes a CM_PAINTBACKGROUND message for only that *
 *                 record. This is difficult to handle so it is not  *
 *                 being processed correctly at this time. As a      *
 *                 result, when this happens, the bitmap is          *
 *                 compressed to the size of the item and drawn. Not *
 *                 a great effect but to fix this I'd have to store  *
 *                 a bitmap for each container window and bitblt     *
 *                 from an offset from that bitmap. If this were     *
 *                 anything but a sample, I'd do it <g>.             *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  VOID    DrawSubclassCnr( HWND hwndClient );                      *
 *  MRESULT DrawItem( HWND hwndClient, POWNERITEM poi );             *
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

#define  INCL_GPIBITMAPS
#define  INCL_GPIPRIMITIVES
#define  INCL_WINDIALOGS
#define  INCL_WINERRORS
#define  INCL_WINFRAMEMGR
#define  INCL_WINMENUS
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

static VOID PaintBackground( PINSTANCE pi, POWNERBACKGROUND pob );

FNWP wpCnr;

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*------------------------- DrawSubclassCnr --------------------------*/
/*                                                                    */
/*  SUBCLASS THE CONTAINER SO WE CAN GET CM_PAINTBACKGROUND.          */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID DrawSubclassCnr( HWND hwndClient )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    HWND      hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );

    if( !pi )
    {
        Msg( "DrawSubclass cant get Inst data. RC(%X)", HWNDERR( hwndClient ) );

        return;
    }

    // Subclass the container so we can capture the CM_PAINTBACKGROUND message

    pi->pfnwpDefCnr = WinSubclassWindow( hwndCnr, wpCnr );

    if( !pi->pfnwpDefCnr )
        Msg( "DrawSubclassCnr WinSubclassWindow RC(%X)",HWNDERR( hwndClient ) );

}

/**********************************************************************/
/*---------------------------- DrawItem ------------------------------*/
/*                                                                    */
/*  PROCESS A WM_DRAWITEM MESSAGE FOR THE CONTAINER.                  */
/*                                                                    */
/*  INPUT: client window handle                                       */
/*         pointer to an OWNERITEM structure                          */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: result of drawing                                         */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
MRESULT DrawItem( HWND hwndClient, POWNERITEM poi )
{
    PCNRDRAWITEMINFO pcdii = (PCNRDRAWITEMINFO) poi->hItem;
    CHAR             szAttrs[ 6 ];
    PCNRITEM         pci = (PCNRITEM) pcdii->pRecord;
    PCH              pch = szAttrs;
    COLOR            clrForeground = CLR_RED;

    // Let the container draw the column heading

    if( !pci )
        return (MRESULT) FALSE;

    // We placed this column identifier in pUserData when we set up the columns
    // in create.c. This is just a number that lets us know what column we are
    // in when all we have is a FIELDINFO pointer. The only column that is
    // declared CFA_OWNER is the FILEATTR_COLUMN column, but this is used
    // primarily to point out what methods are available to determine which
    // column we are in if there is more than one ownerdraw column.

    if( pcdii->pFieldInfo->pUserData != (PVOID) FILEATTR_COLUMN )
        return (MRESULT) FALSE;

    (void) memset( szAttrs, 0, sizeof( szAttrs ) );

    if( pci->attrFile & FILE_DIRECTORY )
        *(pch++) = 'D';

    if( pci->attrFile & FILE_READONLY )
        *(pch++) = 'R';

    if( pci->attrFile & FILE_HIDDEN )
        *(pch++) = 'H';

    if( pci->attrFile & FILE_SYSTEM )
        *(pch++) = 'S';

    if( pci->attrFile & FILE_ARCHIVED )
        *(pch++) = 'A';

    // If record is selected, simulate CRA_SELECTED emphasis. It appears to be
    // up to us to draw the selection state.

    if( poi->fsAttribute & CRA_SELECTED )
    {
        clrForeground = SYSCLR_HILITEFOREGROUND;

        if( !WinFillRect( poi->hps, &(poi->rclItem), SYSCLR_HILITEBACKGROUND ) )
            Msg( "DrawItem WinFillRect RC(%X)", HWNDERR( hwndClient ) );
    }

    poi->rclItem.xLeft += 10;

    if( *szAttrs )
        if( !WinDrawText( poi->hps, strlen( szAttrs ), szAttrs, &(poi->rclItem),
                          clrForeground, 0, DT_VCENTER | DT_LEFT ) )
            Msg( "DrawItem WinDrawText RC(%X)", HWNDERR( hwndClient ) );

    return (MRESULT) TRUE;
}

/**********************************************************************/
/*------------------------------ wpCnr -------------------------------*/
/*                                                                    */
/*  CONTAINER WINDOW PROCEDURE (SUBCLASSED).                          */
/*                                                                    */
/*  INPUT: standard window procedure parameters                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: result of message processing                              */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
MRESULT EXPENTRY wpCnr( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    PINSTANCE pi = INSTDATA( PARENT( hwnd ) );

    if( !pi )
    {
        Msg( "wpCnr cant get Inst data. RC(%X)", HWNDERR( hwnd ) );

        return 0;
    }

    switch( msg )
    {
        case CM_PAINTBACKGROUND:
        {
            PaintBackground( pi, (POWNERBACKGROUND) mp1 );

            return (MRESULT) TRUE;
        }
    }

    return pi->pfnwpDefCnr( hwnd, msg, mp1, mp2 );
}

/**********************************************************************/
/*------------------------- PaintBackground --------------------------*/
/*                                                                    */
/*  PROCESS THE CM_PAINTBACKGROUND MESSAGE.                           */
/*                                                                    */
/*  INPUT: pointer to OWNERBACKGROUND structure                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID PaintBackground( PINSTANCE pi, POWNERBACKGROUND pob )
{
    // clrBackground is initially set to CLR_WHITE during window initialization.
    // The user can change this by selecting a color from the Background
    // submenu. If they choose Bitmap from that submenu, we set clrBackground
    // to 0, meaning we should draw the bitmap. This bitmap was created in
    // ProgInit in CNRADV.C. It is destroyed in ProgTerm in that same module.

    if( pi->clrBackground )
    {
        if( !WinFillRect( pob->hps, &(pob->rclBackground), pi->clrBackground ) )
            Msg( "PaintBackground WinFillRect RC(%X)", HWNDERR( pob->hwnd ) );
    }
    else
        if( !WinDrawBitmap( pob->hps, hbmBackground, NULL,
                          (PPOINTL) &(pob->rclBackground), 0, 0, DBM_STRETCH ) )
            Msg( "PaintBackground WinDrawBitmap RC(%X)",
                 HWNDERR( pob->hwnd ) );
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
