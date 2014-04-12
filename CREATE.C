/*********************************************************************
 *                                                                   *
 * MODULE NAME :  create.c               AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It performs the function of   *
 *  creating the container window and starting a thread that will    *
 *  populate the window with file icons.                             *
 *                                                                   *
 *  The primary purpose of this module is to create the container    *
 *  and set it up to be able to switch views easily. This is         *
 *  accomplished by creating the column definitions for the Details  *
 *  view. The Details view is really the only view that requires     *
 *  any substantial setup.                                           *
 *                                                                   *
 *  We also set the base directory here. This directory will be      *
 *  traversed in the PopulateContainer thread as will all its        *
 *  subdirectories. If the user entered a directory name on the      *
 *  command line we don't need to do anything. If not, we get the    *
 *  current drive and directory and use it as the base.              *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *   HWND CreateDirectoryWin( PSZ szDirectory, PCNRITEM pciParent ); *
 *   HWND CreateContainer( HWND hwndClient, PSZ szDirectory );       *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  11-30-92 - Source copied from CNRMENU.EXE sample.                *
 *             Added FCF_ICON to frame flags.                        *
 *  12-05-92 - Added call to DrawSubclassCnr in CreateContainer.     *
 *             Created a static FIELDINFO initializer array.         *
 *             Used the initializer array in SetContainerColumns.    *
 *             Added Attributes column to demonstrate Ownerdraw.     *
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
#include <process.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cnradv.h"

/*********************************************************************/
/*------------------- APPLICATION DEFINITIONS -----------------------*/
/*********************************************************************/

#define FRAME_FLAGS         (FCF_TASKLIST | FCF_TITLEBAR   | FCF_SYSMENU | \
                             FCF_MINMAX   | FCF_SIZEBORDER | FCF_ICON    | \
                             FCF_SHELLPOSITION)

#define SPLITBAR_OFFSET     150         // Pixel offset of details splitbar

#define THREAD_STACKSIZE    65536       // Stacksize for secondary thread

/**********************************************************************/
/*---------------------------- STRUCTURES ----------------------------*/
/**********************************************************************/

/**********************************************************************/
/*----------------------- FUNCTION PROTOTYPES ------------------------*/
/**********************************************************************/

static BOOL SetContainerColumns  ( HWND hwndCnr );
static VOID GetCurrentDirectory  ( PSZ pszDirectory );
static VOID UseCmdLineDirectory  ( PSZ pszDirectoryOut, PSZ szDirectoryIn );

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

FIELDINFO fldinfo[] =    // INFORMATION ABOUT EACH CONTAINER COLUMN
{
    { 0, CFA_BITMAPORICON | CFA_CENTER | CFA_HORZSEPARATOR | CFA_SEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "Icon",
      offsetof( CNRITEM, hptrIcon ), (PVOID) ICON_COLUMN },

    { 0, CFA_STRING | CFA_LEFT | CFA_HORZSEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "File Name",
      offsetof( CNRITEM, pszFileName ), (PVOID) FILENAME_COLUMN },

    { 0, CFA_ULONG | CFA_RIGHT | CFA_HORZSEPARATOR | CFA_SEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "File Size",
      offsetof( CNRITEM, cbFile ), (PVOID) FILESIZE_COLUMN },

    { 0, CFA_DATE | CFA_RIGHT | CFA_HORZSEPARATOR | CFA_SEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "Date",
      offsetof( CNRITEM, date ), (PVOID) DATE_COLUMN },

    { 0, CFA_TIME | CFA_RIGHT | CFA_HORZSEPARATOR | CFA_SEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "Time",
      offsetof( CNRITEM, time ), (PVOID) TIME_COLUMN },

    { 0, CFA_OWNER | CFA_HORZSEPARATOR,
      CFA_CENTER | CFA_FITITLEREADONLY, "Attributes\n(Ownerdraw)",
      0, (PVOID) FILEATTR_COLUMN }
};

#define COLUMN_COUNT (sizeof( fldinfo ) / sizeof( FIELDINFO ))

/**********************************************************************/
/*----------------------- CreateDirectoryWin -------------------------*/
/*                                                                    */
/*  CREATE A DIRECTORY WINDOW MADE UP OF FRAME/CLIENT/CONTAINER.      */
/*                                                                    */
/*  INPUT: directory to represent in the window,                      */
/*         window handle with which to share records, or NULLHANDLE,  */
/*         pointer to CNRITEM if using shared records, or NULL        */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: window handle of created frame or NULLHANDLE if error     */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
HWND CreateDirectoryWin( PSZ szDirectory, HWND hwndCnrShare, PCNRITEM pciParent)
{
    FRAMECDATA  fcdata;
    HWND        hwndFrame, hwndClient = NULLHANDLE;

    (void) memset( &fcdata, 0, sizeof( FRAMECDATA ) );

    fcdata.cb               = sizeof( FRAMECDATA );
    fcdata.flCreateFlags    = FRAME_FLAGS;
    fcdata.idResources      = ID_RESOURCES;

    // Create the frame window. If we were creating multiple instances of
    // this window, iWinCount would become useful.

    hwndFrame = WinCreateWindow( HWND_DESKTOP, WC_FRAME, NULL, 0, 0, 0, 0, 0,
                                 NULLHANDLE, HWND_TOP,
                                 ID_FIRST_DIRWINDOW + iWinCount, &fcdata,
                                 NULL );

    // Create the client window which will create the container in its
    // WM_CREATE processing. If that fails, hwndClient will be NULLHANDLE.
    // We pass parameters in the CTLDATA parameter which will be received by the
    // client in mp1 on WM_CREATE.

    if( hwndFrame )
    {
        // This memory block will be freed after WM_CREATE processing is
        // complete by InitClient in cnradv.c

        PWINCREATE pwc = (PWINCREATE) malloc( sizeof( WINCREATE ) );

        if( pwc )
        {
            pwc->szDirectory  = szDirectory;
            pwc->hwndCnrShare = hwndCnrShare;
            pwc->pciParent    = pciParent;

            hwndClient = WinCreateWindow( hwndFrame, DIRECTORY_WINCLASS, NULL,
                                          0, 0, 0, 0, 0, NULLHANDLE, HWND_TOP,
                                          FID_CLIENT, pwc, NULL );
        }
        else
            Msg( "Out of memory in CreateContainer!" );
    }

    if( hwndClient )
    {
        // Show the frame window (it was created invisible)

        if( !WinSetWindowPos( hwndFrame, HWND_TOP, 0, 0, 0, 0,
                              SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER ) )
        {
            hwndFrame = NULLHANDLE;

            Msg( "hwndFrame WinSetWindowPos RC(%X)", HWNDERR( hwndClient ) );
        }
    }
    else
        hwndFrame = NULLHANDLE;

    return hwndFrame;
}

/**********************************************************************/
/*------------------------- CreateContainer --------------------------*/
/*                                                                    */
/*  CREATE THE CONTAINER AND CUSTOMIZE IT.                            */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         directory to display in the container,                     */
/*         window handle with which to share records, or NULLHANDLE,  */
/*         pointer to CNRITEM if using shared records, or NULL        */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: Container window handle                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
HWND CreateContainer( HWND hwndClient, PSZ szDirectory, HWND hwndCnrShare,
                      PCNRITEM pciParent )
{
    PINSTANCE pi = INSTDATA( hwndClient );
    HWND      hwndCnr = NULLHANDLE;

    if( !pi )
    {
        Msg( "CreateContainer cant get Inst data. RC(%X)", HWNDERR(hwndClient));

        return NULLHANDLE;
    }

    // Create the container control. Indicate that we want to use
    // MINIRECORDCORE structures rather than RECORDCORE structures. This
    // saves memory. Also allow extended selection which essentially lets the
    // user select multiple items.

    hwndCnr = WinCreateWindow( hwndClient, WC_CONTAINER, NULL,
                               CCS_EXTENDSEL | CCS_MINIRECORDCORE | WS_VISIBLE,
                               0, 0, 0, 0, hwndClient, HWND_TOP, CNR_DIRECTORY,
                               NULL, NULL);

    if( hwndCnr )
    {
        // Subclass the container to get the CM_PAINTBACKGROUND message
        // (function is in draw.c)

        DrawSubclassCnr( hwndClient );

        if( SetContainerColumns( hwndCnr ) )
        {
            // Start the thread that will populate the container. Allocate
            // memory to pass the thread a structure of data.

            PTHREADPARMS ptp = malloc( sizeof( THREADPARMS ) );

            if( ptp )
            {
                TID tid = 0;

                ptp->hwndClient     = hwndClient;
                ptp->hwndCnrShare   = hwndCnrShare;
                ptp->pciParent      = pciParent;

                // If no directory was passed to us, assume that the current
                // directory is to be displayed.

                if( !szDirectory )
                    GetCurrentDirectory( pi->szDirectory );
                else
                    UseCmdLineDirectory( pi->szDirectory, szDirectory );

                SetWindowTitle( hwndClient, "%s: Processing %s...",
                                PROGRAM_TITLE, pi->szDirectory );

                // Start the thread that will populate the container with
                // file icons. This is a separate thread because it could
                // take a while if there are many subdirectories.

                tid = _beginthread( PopulateContainer, NULL,
                                    THREAD_STACKSIZE, (PVOID) ptp );

                if( (INT) tid == -1 )
                    Msg( "Cant start PopulateContainer thread!" );
            }
            else
            {
                hwndCnr = NULLHANDLE;

                Msg( "Out of Memory in CreateContainer!" );
            }
        }
        else
            hwndCnr = NULLHANDLE;
    }
    else
        Msg( "Couldnt create container. RC(%X)", HWNDERR( hwndClient ) );

    return hwndCnr;
}

/**********************************************************************/
/*----------------------- SetContainerColumns ------------------------*/
/*                                                                    */
/*  SET THE COLUMNS OF THE CONTAINER FOR DETAIL VIEW                  */
/*                                                                    */
/*  INPUT: container window handle                                    */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if successful or not                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL SetContainerColumns( HWND hwndCnr )
{
    BOOL        fSuccess = TRUE;
    PFIELDINFO  pfi, pfiLastLeftCol;

    // Allocate storage for container column data

    pfi = WinSendMsg( hwndCnr, CM_ALLOCDETAILFIELDINFO,
                      MPFROMLONG( COLUMN_COUNT ), NULL );

    if( pfi )
    {
        PFIELDINFO      pfiFirst;
        FIELDINFOINSERT fii;
        INT             i;

        // Store original value of pfi so we won't lose it when it changes.
        // This will be needed on the CM_INSERTDETAILFIELDINFO message.

        pfiFirst = pfi;

        // Fill in the column information from a static array at the top of
        // this module

        for( i = 0; i < COLUMN_COUNT; i++ )
        {
            // Fill in values that were returned from CM_ALLOCDETAILFIELDINFO

            fldinfo[ i ].cb = pfi->cb;

            fldinfo[ i ].pNextFieldInfo = pfi->pNextFieldInfo;

            // Copy filled-in structure to container memory

            *pfi = fldinfo[ i ];

            // The last column before the splitbar is the filename column

            if( pfi->offStruct == FIELDOFFSET( CNRITEM, pszFileName ) )
                pfiLastLeftCol = pfi;

            pfi = pfi->pNextFieldInfo;
        }

        // Use the CM_INSERTDETAILFIELDINFO message to tell the container
        // all the column information it needs to function properly. Place
        // this column info first in the column list and update the display
        // after they are inserted (fInvalidateFieldInfo = TRUE)

        (void) memset( &fii, 0, sizeof( FIELDINFOINSERT ) );

        fii.cb                   = sizeof( FIELDINFOINSERT );
        fii.pFieldInfoOrder      = (PFIELDINFO) CMA_FIRST;
        fii.cFieldInfoInsert     = (SHORT) COLUMN_COUNT;
        fii.fInvalidateFieldInfo = TRUE;

        if( !WinSendMsg( hwndCnr, CM_INSERTDETAILFIELDINFO, MPFROMP( pfiFirst ),
                         MPFROMP( &fii ) ) )
        {
            fSuccess = FALSE;

            Msg( "CM_INSERTDETAILFIELDINFO RC(%X)", HWNDERR( hwndCnr ) );
        }
    }
    else
    {
        fSuccess = FALSE;

        Msg( "CM_ALLOCDETAILFIELDINFO failed. RC(%X)", HWNDERR( hwndCnr ) );
    }

    if( fSuccess )
    {
        CNRINFO cnri;

        // Tell the container about the splitbar and where it goes

        cnri.cb             = sizeof( CNRINFO );
        cnri.pFieldInfoLast = pfiLastLeftCol;
        cnri.xVertSplitbar  = SPLITBAR_OFFSET;

        if( !WinSendMsg( hwndCnr, CM_SETCNRINFO, MPFROMP( &cnri ),
                        MPFROMLONG( CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR ) ) )
        {
            fSuccess = FALSE;

            Msg( "SetContainerColumns CM_SETCNRINFO RC(%X)", HWNDERR(hwndCnr) );
        }
    }

    return fSuccess;
}

/**********************************************************************/
/*----------------------- GetCurrentDirectory ------------------------*/
/*                                                                    */
/*  GET THE CURRENT DIRECTORY AND STORE IT.                           */
/*                                                                    */
/*  INPUT: pointer to buffer in which to place path info              */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID GetCurrentDirectory( PSZ pszDirectory )
{
    ULONG  cbDirPath = CCHMAXPATH;
    APIRET rc;
    ULONG  ulCurrDrive, ulDrvMap;

    // Set up the "d:\" string to which we will add the current directory.
    // If we can get the current drive number, use it. Otherwise default to
    // C:

    rc = DosQueryCurrentDisk( &ulCurrDrive, &ulDrvMap );

    if( !rc )
    {
        pszDirectory[ 0 ] = ulCurrDrive + ('A' - 1);

        (void) strcpy( pszDirectory + 1, ":\\" );
    }
    else
        (void) strcpy( pszDirectory, "C:\\" );

    rc = DosQueryCurrentDir( 0, pszDirectory + 3, &cbDirPath );

    // If good retcode, DosQueryCurrentDir just returns the current
    // directory without a trailing backslash. Of course there are always
    // complications. The complication here is that if the current directory is
    // the root, the string *will* end with a backslash. So if the trailing
    // character is a backslash, remove it so we have a pure directory name.

    if( pszDirectory[ strlen( pszDirectory ) - 1 ] == '\\' )
        pszDirectory[ strlen( pszDirectory ) - 1 ] = 0;

    if( rc )
        Msg( "DosQueryCurrentDir RC(%X). Using Root Directory.", rc );
}

/**********************************************************************/
/*----------------------- UseCmdLineDirectory ------------------------*/
/*                                                                    */
/*  TAKE THE DIRECTORY FROM THE COMMANDLINE AND VALIDATE IT.          */
/*                                                                    */
/*  INPUT: pointer to buffer in which to place correct path,          */
/*         directory input at the commandline                         */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID UseCmdLineDirectory( PSZ pszDirectoryOut, PSZ szDirectoryIn )
{
    (void) strcpy( pszDirectoryOut, szDirectoryIn );

    // If the user ended their directory name with a backslash, take it off
    // because this program needs to have just the base directory name here.

    if( pszDirectoryOut[ strlen( pszDirectoryOut ) - 1 ] == '\\' )
        pszDirectoryOut[ strlen( pszDirectoryOut ) - 1 ] = 0;
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
