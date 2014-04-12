/*********************************************************************
 *                                                                   *
 * MODULE NAME :  common.c               AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It contains common functions  *
 *  used by all modules in the EXE.                                  *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  VOID SetWindowTitle( HWND hwndClient, PSZ szFormat, ... );       *
 *  VOID Msg           ( PSZ szFormat, ... );                        *
 *  VOID FullyQualify  ( PSZ szBuf, HWND hwndCnr, PCNRITEM pci );    *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  11-30-92 - Source copied from CNRMENU.EXE sample.                *
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

#define  INCL_WINERRORS
#define  INCL_WINFRAMEMGR
#define  INCL_WINMENUS
#define  INCL_WINSTDCNR
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

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*-------------------------- SetWindowTitle --------------------------*/
/*                                                                    */
/*  SET THE FRAME WINDOW'S TITLEBAR TEXT.                             */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         a message in printf format with its parms                  */
/*                                                                    */
/*  1. Format the message using vsprintf.                             */
/*  2. Set the text into the titlebar.                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/

#define TITLEBAR_TEXTLEN (CCHMAXPATH + 50)

VOID SetWindowTitle( HWND hwndClient, PSZ szFormat,... )
{
    PSZ     szMsg;
    va_list argptr;

    if( (szMsg = (PSZ) malloc( TITLEBAR_TEXTLEN )) == NULL )
    {
        DosBeep( 1000, 1000 );

        return;
    }

    va_start( argptr, szFormat );

    vsprintf( szMsg, szFormat, argptr );

    va_end( argptr );

    szMsg[ TITLEBAR_TEXTLEN - 1 ] = 0;

    (void) WinSetWindowText( PARENT( hwndClient ), szMsg );

    free( szMsg );
}

/**********************************************************************/
/*------------------------------- Msg --------------------------------*/
/*                                                                    */
/*  DISPLAY A MESSAGE TO THE USER.                                    */
/*                                                                    */
/*  INPUT: a message in printf format with its parms                  */
/*                                                                    */
/*  1. Format the message using vsprintf.                             */
/*  2. Sound a warning sound.                                         */
/*  3. Display the message in a message box.                          */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/

#define MESSAGE_SIZE 1024

VOID Msg( PSZ szFormat,... )
{
    PSZ     szMsg;
    va_list argptr;

    if( (szMsg = (PSZ) malloc( MESSAGE_SIZE )) == NULL )
    {
        DosBeep( 1000, 1000 );

        return;
    }

    va_start( argptr, szFormat );

    vsprintf( szMsg, szFormat, argptr );

    va_end( argptr );

    szMsg[ MESSAGE_SIZE - 1 ] = 0;

    fprintf( stderr, "\n%s", szMsg );

    (void) WinAlarm( HWND_DESKTOP, WA_WARNING );

    (void) WinMessageBox(  HWND_DESKTOP, HWND_DESKTOP, szMsg,
                           PROGRAM_TITLE, 1, MB_OK | MB_MOVEABLE );

    free( szMsg );
}

/**********************************************************************/
/*--------------------------- FullyQualify ---------------------------*/
/*                                                                    */
/*  RECURSIVELY BUILD A FULLY QUALIFIED DIRECTORY NAME.               */
/*                                                                    */
/*  INPUT: directory name buffer,                                     */
/*         container window handle,                                   */
/*         pointer to current CNRITEM container record (subdirectory) */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID FullyQualify( PSZ szDirectory, HWND hwndCnr, PCNRITEM pci )
{
    PCNRITEM pciParent = WinSendMsg( hwndCnr, CM_QUERYRECORD, MPFROMP( pci ),
                                     MPFROM2SHORT(CMA_PARENT, CMA_ITEMORDER) );

    if( (INT) pciParent == -1 )
        Msg( "FullyQualify... CM_QUERYRECORD RC(%X)", HWNDERR( hwndCnr ) );
    else
    {
        if( pciParent )
            FullyQualify( szDirectory, hwndCnr, pciParent );

        (void) strcat( szDirectory, "\\" );

        (void) strcat( szDirectory, pci->szFileName );
    }
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
