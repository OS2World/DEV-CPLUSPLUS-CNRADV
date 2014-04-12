/*********************************************************************
 *                                                                   *
 * MODULE NAME :  populate.c             AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It performs the function of   *
 *  filling a container window with file icons. This processing is   *
 *  taking place in a secondary thread that was started with         *
 *  _beginthread from the CreateContainer function in create.c.      *
 *                                                                   *
 *  Recursion is used to fill the container with all subdirectories  *
 *  from the base directory.  This is done to demonstrate the Tree   *
 *  view.                                                            *
 *                                                                   *
 *  The reason this is in a separate thread is that, if we are       *
 *  traversing the root directory and its subdirectories, it could   *
 *  take a long time to fill the container.                          *
 *                                                                   *
 *  This thread posts a UM_CONTAINER_FILLED message to the client    *
 *  window when the container is filled.                             *
 *                                                                   *
 *  The first container created in this test program will actually   *
 *  go to the file system to get the files and will allocate memory  *
 *  for the container records. All other containers will use shared  *
 *  records from the first container. This necessitates different    *
 *  functions for each case. The first container uses                *
 *  ProcessDirectory to fill the container. All additional containers*
 *  use InsertSharedDir to do this.                                  *
 *                                                                   *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  VOID PopulateContainer( PVOID pThreadParms );                    *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  11-30-92 - Source copied from CNRMENU.EXE sample.                *
 *             Use global folder icon for directories in the         *
 *               FillInRecord function.                              *
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

#define  INCL_DOSERRORS
#define  INCL_DOSFILEMGR
#define  INCL_DOSPROCESS
#define  INCL_WINERRORS
#define  INCL_WINFRAMEMGR
#define  INCL_WINMENUS
#define  INCL_WINPOINTERS
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

#define FILES_TO_GET       100        // Nbr of files to search for at a time
#define FF_BUFFSIZE        (sizeof( FILEFINDBUF3 ) * FILES_TO_GET)

/**********************************************************************/
/*---------------------------- STRUCTURES ----------------------------*/
/**********************************************************************/

/**********************************************************************/
/*----------------------- FUNCTION PROTOTYPES ------------------------*/
/**********************************************************************/

static VOID ProcessDirectory ( HAB habThread, HWND hwndCnr, PCNRITEM pciParent,
                               PSZ szDirBase, PSZ szDirectory );
static VOID RecurseSubdirs   ( HAB habThread, HWND hwndCnr, PCNRITEM pciParent,
                               PSZ szDir );
static BOOL InsertRecords    ( HAB habThread, HWND hwndCnr, PCNRITEM pciParent,
                               PSZ szDir, PFILEFINDBUF3 pffb, ULONG cFiles,
                               PINT piDirPosition );
static BOOL FillInRecord     ( PCNRITEM pci, PSZ szDir, PFILEFINDBUF3 pffb,
                               INT iDirPosition );
static VOID InsertSharedDir  ( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                               PCNRITEM pciShrParent, PCNRITEM pciParent );
static VOID RecurseSharedDirs( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                               PCNRITEM pciParent );
static BOOL InsertSharedRecs ( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                               PCNRITEM pciShrParent, PCNRITEM pciParent );

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*------------------------ PopulateContainer -------------------------*/
/*                                                                    */
/*  THREAD THAT FILLS THE CONTAINER WITH RECORDS.                     */
/*                                                                    */
/*  INPUT: pointer to thread parameters passed by main thread         */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID PopulateContainer( PVOID pThreadParms )
{
    HAB         hab;
    HMQ         hmq = NULLHANDLE;
    HWND        hwndClient = ((PTHREADPARMS) pThreadParms)->hwndClient;
    HWND        hwndCnrShare = ((PTHREADPARMS) pThreadParms)->hwndCnrShare;
    PCNRITEM    pciParent = ((PTHREADPARMS) pThreadParms)->pciParent;
    PINSTANCE   pi = INSTDATA( hwndClient );

    // We must create a message queue so that this thread can do WinSendMsg's.
    // All contact with the container is done with WinSendMsg.

    hab = WinInitialize( 0 );

    if( hab )
        hmq = WinCreateMsgQueue( hab, 0 );
    else
        Msg( "PopulateContainer WinInitialize failed!" );

    if( hmq )
    {
        if( pi )
        {
            if( hwndCnrShare && pciParent )

            // If we were passed the above info, that means we are to get our
            // container records from another container rather than allocating
            // the record memory ourself. The NULL in the last parameter tells
            // the new container to start at the top level of the tree even
            // though it is using a subdirectory. pciParent contains the
            // record from the old container that points to the starting place
            // of the new container

                InsertSharedDir( hab, hwndCnrShare,
                                 WinWindowFromID( hwndClient, CNR_DIRECTORY ),
                                 pciParent, NULL );
            else

            // Insert the container records from the specified directory. Using
            // NULL for the parent record tells ProcessDirectory that it is
            // dealing with the top-level directory should recursion cause
            // subdirectories to be expanded. The last parameter is a pointer
            // to a subdirectory used during recursion and can be NULL here.

                ProcessDirectory( hab,
                                  WinWindowFromID( hwndClient, CNR_DIRECTORY ),
                                  NULL, pi->szDirectory, NULL );
        }
        else
            Msg( "PopulateContainer cant get Inst data. RC(%X)", HABERR( hab ));
    }
    else
        Msg( "PopulateContainer CreateMsgQueue failed! RC(%X)", HABERR( hab ) );

    if( hmq )
        (void) WinDestroyMsgQueue( hmq );

    if( hab )
        (void) WinTerminate( hab );

    free( pThreadParms );

    // Let the primary thread know the container is filled

    WinPostMsg( hwndClient, UM_CONTAINER_FILLED, NULL, NULL );

    _endthread();
}

/**********************************************************************/
/*------------------------- ProcessDirectory -------------------------*/
/*                                                                    */
/*  POPULATE THE CONTAINER WITH THE CONTENTS OF A DIRECTORY           */
/*                                                                    */
/*  INPUT: anchor block handle for this thread,                       */
/*         container window handle,                                   */
/*         parent container record,                                   */
/*         base directory name with drive qualifier,                  */
/*         directory to display                                       */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID ProcessDirectory( HAB hab, HWND hwndCnr, PCNRITEM pciParent,
                              PSZ szDirBase, PSZ szDirectory )
{
    // Allocate a buffer big enough to hold FILES_TO_GET files. Then allocate
    // a work buffer to hold the full file spec.

    PFILEFINDBUF3 pffb = malloc( FF_BUFFSIZE );
    PSZ           szFileSpec = malloc( CCHMAXPATH + 1 );
    INT           iDirPosition = 0;

    if( pffb && szFileSpec )
    {
        HDIR   hdir = HDIR_SYSTEM;
        ULONG  ulMaxFiles = FILES_TO_GET;
        PCH    pchEndPath;
        APIRET rc;
        PINSTANCE pi = INSTDATA( PARENT( hwndCnr ) );

        // Combine C:\DIR1\DIR2 and DIR3 to make C:\DIR1\DIR2\DIR3\*.*
        // Keep a placeholder so we can strip the trailing '\*.*' after the
        // DosFindFirst has completed.

        (void) strcpy( szFileSpec, szDirBase );

        if( szDirectory )
        {
            (void) strcat( szFileSpec, "\\" );

            (void) strcat( szFileSpec, szDirectory );
        }

        pchEndPath = szFileSpec + strlen( szFileSpec );

        (void) strcat( szFileSpec, "\\*.*" );

        // Get buffer of files up to the maximum FILES_TO_GET files. Get both
        // normal files and directories.

        rc = DosFindFirst( szFileSpec, &hdir, FILE_NORMAL | FILE_DIRECTORY,
                           pffb, FF_BUFFSIZE, &ulMaxFiles, FIL_STANDARD );

        *pchEndPath = 0;

        // Let the user know what directory we're processing unless we're
        // in the process of shutting down

        if( pi && !pi->fShutdown )
            SetWindowTitle( PARENT( hwndCnr ), "%s: Processing %s...",
                            PROGRAM_TITLE, szFileSpec );

        while( !rc )
        {
            // If the main thread wants to shutdown, accommodate it

            if( pi && pi->fShutdown )
                break;

            // Insert the files into the container. Pass a pointer to the int
            // that keeps track of the relative position of the file within the
            // directory. This variable will be incremented for every record
            // inserted into the container. We do this so the user can sort by
            // a different key and at a later time get back to this order by
            // sorting by this variable.

            if( InsertRecords( hab, hwndCnr, pciParent, szFileSpec, pffb,
                               ulMaxFiles, &iDirPosition ) )

                // Get more files if there are any

                rc = DosFindNext( hdir, pffb, FF_BUFFSIZE, &ulMaxFiles );
            else
                rc = 1;
        }

        DosFindClose( hdir );

        // Give up our timeslice so we don't monopolize the cpu

        DosSleep( 5 );

        // Recursively insert child records into the container for any
        // subdirectories found under this directory

        if( pi && !pi->fShutdown )
            RecurseSubdirs( hab, hwndCnr, pciParent, szFileSpec );
    }
    else
        Msg( "ProcessDirectory Out of Memory!" );

    if( pffb )
        free( pffb );

    if( szFileSpec )
        free( szFileSpec );
}

/**********************************************************************/
/*-------------------------- RecurseSubdirs --------------------------*/
/*                                                                    */
/*  CALL ProcessDirectory FOR EACH SUBDIRECTORY OF THE BASE DIRECTORY */
/*                                                                    */
/*  INPUT: anchor block handle for this thread,                       */
/*         container window handle,                                   */
/*         parent container record,                                   */
/*         base directory name with drive qualifier                   */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID RecurseSubdirs( HAB hab, HWND hwndCnr, PCNRITEM pciParent,
                            PSZ szDirBase )
{
    USHORT    usWhatRec = (pciParent == NULL) ? CMA_FIRST : CMA_FIRSTCHILD;
    PCNRITEM  pciPrev = pciParent, pciNext;
    PINSTANCE pi = INSTDATA( PARENT( hwndCnr ) );

    // Note that this function is called AFTER a subdirectory's files have
    // been inserted into the container. We traverse those records to see if
    // any are subdirectories themselves.

    for( ; ; )
    {
        // If the main thread wants to shutdown, accommodate it

        if( pi && pi->fShutdown )
            break;

        // Get the next child record. We start with usWhatRec set to
        // CMA_FIRSTCHILD, then use CMA_NEXT after getting the first child.
        // This enumerates the children of pciParent. The docs don't
        // go into it, but my tests show that using CMA_NEXT after a
        // CMA_FIRSTCHILD will return NULL after the last *child* has been
        // enumerated, rather than the last record in the container. Of course
        // that is how it should work but it's not documented.

        // BUG NOTE (10/19/92): I had to use CMA_FIRST instead of CMA_FIRSTCHILD
        // if pciParent is NULL (parent is the top container level). This bug
        // is fixed in the Service Pack due any day now

        pciNext = WinSendMsg( hwndCnr, CM_QUERYRECORD, MPFROMP( pciPrev ),
                              MPFROM2SHORT( usWhatRec, CMA_ITEMORDER ) );

        if( (INT) pciNext == -1 )
        {
            Msg( "RecurseSubdirs CM_QUERYRECORD RC(%X)", HABERR( hab ) );

            break;
        }

        if( !pciNext )
            break;

        // If we found a subdirectory that isn't '.' or '..', recursively
        // call PopulateContainer for that subdirectory (remember, while we're
        // in this function, the parent's ProcessDirectory hasn't completed
        // yet)

        if( (pciNext->attrFile & FILE_DIRECTORY) &&
             pciNext->szFileName[0] != '.' )
            ProcessDirectory( hab, hwndCnr, pciNext, szDirBase,
                              pciNext->szFileName );

        usWhatRec = CMA_NEXT;

        pciPrev = pciNext;
    }
}

/**********************************************************************/
/*--------------------------- InsertRecords --------------------------*/
/*                                                                    */
/*  INSERT DIRECTORY ENTRIES INTO THE CONTAINER.                      */
/*                                                                    */
/*  INPUT: anchor block handle for this thread,                       */
/*         container window handle,                                   */
/*         parent container record,                                   */
/*         directory being displayed,                                 */
/*         buffer containing directory entries,                       */
/*         count of files in directory buffer,                        */
/*         pointer to relative position of this file in the directory */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if successful or not                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL InsertRecords( HAB hab, HWND hwndCnr, PCNRITEM pciParent,
                           PSZ szDirectory, PFILEFINDBUF3 pffb, ULONG cFiles,
                           PINT piDirPosition )
{
    BOOL     fSuccess = TRUE;
    PBYTE    pbBuf = (PBYTE) pffb;
    PCNRITEM pci;

    // Allocate memory for cFiles container records. EXTRA_RECORD_BYTES refers
    // to the number of bytes per record over and above the MINIRECORDCORE
    // structure size that we need per record. Take a look at the PCNRITEM
    // struct in CNRADV.H to see what kind of data we are storing. The good
    // thing is that the container will allocate this for us during the
    // CM_ALLOCRECORD message. When we do a CM_REMOVERECORD during WM_DESTROY
    // processing, we can free all this container memory in 1 shot. Note that
    // CM_ALLOCRECORD allocates a linked list of records and sets the
    // MINIRECORDCORE.cb size field for each record. It also appears to
    // zero out all allocated memory besides this .cb field. It knows to
    // allocate enough memory for MINIRECORDCORE rather than RECORDCORE structs
    // due to using CCS_MINIRECORDCORE on the WinCreateWindow of the container.

    pci = WinSendMsg( hwndCnr, CM_ALLOCRECORD, MPFROMLONG( EXTRA_RECORD_BYTES ),
                      MPFROMLONG( cFiles ) );

    if( pci )
    {
        INT           i;
        PFILEFINDBUF3 pffbFile;
        RECORDINSERT  ri;
        PCNRITEM      pciFirst = pci;
        ULONG         cFilesInserted = cFiles;

        // Insert all files into the container in one shot by filling in each
        // linked list node that the container allocated for us.

        for( i = 0; i < cFiles; i++ )
        {
            // Get next FILEFINDBUF3 structure that points to a found file.

            pffbFile = (PFILEFINDBUF3) pbBuf;

            // Fill in the container record with the file info. Pass the
            // integer that keeps track of the relative position of this file
            // within the directory. We will add this to the CNRITEM struct for
            // this record for later use during a sort by directory order.

            if( FillInRecord( pci, szDirectory, pffbFile, ++(*piDirPosition) ) )

                // Get the next container record in the linked list that the
                // container allocated for us.

                pci = (PCNRITEM) pci->rc.preccNextRecord;
            else
                cFilesInserted--;

            // Point to the next file in the buffer. This is done by adding
            // an offset value to the current location in the buffer. Since
            // the file name is variable length, this offset points to the
            // end of the current file name and the beginning of the next
            // one.

            pbBuf += pffbFile->oNextEntryOffset;
        }

        // Use the RECORDINSERT structure to tell the container how to
        // insert this batch of records. Here we ask to insert the
        // records at the end of the linked list. The parent record indicates
        // who to stick this batch of records under (if pciParent is NULL, the
        // records are at the top level). (Child records are only displayed in
        // Tree view). The zOrder is used for icon view only and specifies the
        // ZORDER that places one record on top of another. In this case we are
        // placing this batch of records at the top of the ZORDER. Also since
        // fInvalidateRecord is TRUE, we will cause the records to be painted
        // as they are inserted. In a container with a small amount of records
        // you probably want to set this to FALSE and do a CM_INVALIDATERECORD
        // (using 0 for cNumRecord) after all records have been inserted. But
        // here the user needs visual feedback if a large amount of
        // subdirectories are found.

        (void) memset( &ri, 0, sizeof( RECORDINSERT ) );

        ri.cb                 = sizeof( RECORDINSERT );
        ri.pRecordOrder       = (PRECORDCORE) CMA_END;
        ri.pRecordParent      = (PRECORDCORE) pciParent;
        ri.zOrder             = (USHORT) CMA_TOP;
        ri.cRecordsInsert     = cFilesInserted;
        ri.fInvalidateRecord  = TRUE;

        if( !WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP( pciFirst ),
                         MPFROMP( &ri ) ) )
        {
            fSuccess = FALSE;

            Msg( "InsertRecords CM_INSERTRECORD RC(%X)", HABERR( hab ) );
        }
    }
    else
    {
        fSuccess = FALSE;

        Msg( "InsertRecords CM_ALLOCRECORD RC(%X)", HABERR( hab ) );
    }

    return fSuccess;
}

/**********************************************************************/
/*-------------------------- FillInRecord ----------------------------*/
/*                                                                    */
/*  POPULATE CONTAINER RECORD WITH FILE INFORMATION                   */
/*                                                                    */
/*  INPUT: pointer to record buffer to fill,                          */
/*         directory path of file,                                    */
/*         pointer to FILEFINDBUF3 that describes the file,           */
/*         relative position of this file in the directory            */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if successful or not                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL FillInRecord( PCNRITEM pci, PSZ szDirectory, PFILEFINDBUF3 pffb,
                          INT iDirPosition )
{
    BOOL     fSuccess = TRUE;
    CHAR     szFullFileName[ CCHMAXPATH + 1 ];
    HPOINTER hptr;

    // Copy the file name into the storage allocated by the container.

    (void) memset( pci->szFileName, 0, sizeof( pci->szFileName ) );
    (void) memcpy( pci->szFileName, pffb->achName, pffb->cchName );

    // Get the fully qualified path for this file

    (void) memset( szFullFileName, 0, sizeof( szFullFileName ) );

    (void) strcpy( szFullFileName, szDirectory );

    szFullFileName[ strlen( szFullFileName ) ] = '\\';

    (void) strcat( szFullFileName, pci->szFileName );

    // WinLoadFileIcon returns a normal file icon for a folder. Since we want
    // our directories to look like the Drives directories, we use a folder
    // icon that we loaded at program init time. This icon is folder.ico and is
    // part of this sample program. It was copied from the Drives folder icon.

    if( pffb->attrFile & FILE_DIRECTORY )
        hptr = hptrFolder;
    else
        // Let PM get the icon for us by using WinLoadFileIcon. Note that a
        // WinFreeFileIcon for this icon is not necessary because we are getting
        // a shared copy by using FALSE as the last parameter. If you do a
        // WinFreeFileIcon on this hptr you will get a PMERR_INVALID_PROCESS_ID.

        hptr = WinLoadFileIcon( szFullFileName, FALSE );

    // WinLoadFileIcon doesn't allow for any error investigation
    // (WinGetLastError always returns zero). It seems to fail when a file is
    // opened in write mode since it fails on the CNRADV debug file and on the
    // .INI files. Use a default icon if it fails.

    if( !hptr )
        hptr = WinQuerySysPointer( HWND_DESKTOP, SPTR_QUESICON, FALSE );

    // Set up the file name pointer to point to the file name. This is
    // crucial because we instructed the container in the
    // SetContainerColumns function (CREATE.C) to use pszFileName as a pointer
    // to the file name (for reasons explained in that function).

    pci->pszFileName    = pci->szFileName;

    // Fill in all fields of the container record.

    pci->hptrIcon       = hptr;
    pci->date.day       = pffb->fdateLastWrite.day;
    pci->date.month     = pffb->fdateLastWrite.month;
    pci->date.year      = pffb->fdateLastWrite.year + 1980;
    pci->time.seconds   = pffb->ftimeLastWrite.twosecs;
    pci->time.minutes   = pffb->ftimeLastWrite.minutes;
    pci->time.hours     = pffb->ftimeLastWrite.hours;
    pci->cbFile         = pffb->cbFile;
    pci->attrFile       = pffb->attrFile;
    pci->iDirPosition   = iDirPosition;

    // Fill in all fields of the MINIRECORDCORE structure. Note that the .cb
    // field of the MINIRECORDCORE struct was filled in by CM_ALLOCRECORD.

    pci->rc.pszIcon     = pci->pszFileName;
    pci->rc.hptrIcon    = hptr;

    return fSuccess;
}

/**********************************************************************/
/*-------------------------- InsertSharedDir -------------------------*/
/*                                                                    */
/*  FILL A NEW CONTAINER WITH THE RECORDS FROM ANOTHER CONTAINER.     */
/*                                                                    */
/*  INPUT: thread's anchor block handle,                              */
/*         window handle of old container that has the records,       */
/*         window handle of new container,                            */
/*         CNRITEM that is the parent record in the old container,    */
/*         CNRITEM that is the parent record in the new container     */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID InsertSharedDir( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                             PCNRITEM pciShrParent, PCNRITEM pciParent )
{
    PINSTANCE pi = INSTDATA( PARENT( hwndCnr ) );

    if( !pi )
    {
        Msg( "InsertSharedDir cant get Inst Data RC(%X)", HABERR( hab ) );

        return;
    }

    SetWindowTitle( PARENT( hwndCnr ), "%s: Processing %s\\%s...",
                    PROGRAM_TITLE, pi->szDirectory,
                    pciParent ? pciParent->szFileName : "" );

    if( InsertSharedRecs( hab, hwndCnrShare, hwndCnr, pciShrParent, pciParent ))
    {
        // If the main thread wants to shutdown, accommodate it

        if( pi->fShutdown )
            return;

        // Invalidate all the records at once if we are at the top level. We
        // do this because we insert the records one at a time and invalidating
        // each individually affects performance. If we are not at the top
        // level, we invalidate individually because the user cannot see that
        // and it is easier. First arrange the records since we may be in
        // Icon view.

        if( !pciParent )
            if( !WinSendMsg( hwndCnr, CM_INVALIDATERECORD, NULL, NULL ) )
                Msg( "InsertSharedDir CM_INVALIDATERECORD RC(%X)", HABERR(hab));

        // Give up our timeslice so we don't monopolize the cpu

        DosSleep( 5 );

        // Now handle subdirectories of this directory

        RecurseSharedDirs( hab, hwndCnrShare, hwndCnr, pciShrParent );
    }
}

/**********************************************************************/
/*------------------------ RecurseSharedDirs -------------------------*/
/*                                                                    */
/*  RECURSIVE FUNCTION THAT INSERTS SHARED RECORDS INTO A CONTAINER.  */
/*                                                                    */
/*  INPUT: thread's anchor block handle,                              */
/*         container window handle containing shared records,         */
/*         container window handle to insert records into,            */
/*         parent record in the new container                         */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static VOID RecurseSharedDirs( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                               PCNRITEM pciParent )
{
    USHORT    usWhatRec = (pciParent==NULL) ? CMA_FIRST : CMA_FIRSTCHILD;
    PCNRITEM  pciPrev = pciParent, pciNext;
    PINSTANCE pi = INSTDATA( PARENT( hwndCnr ) );

    for( ; ; )
    {
        // If the main thread wants to shutdown, accommodate it

        if( pi && pi->fShutdown )
            break;

        // Get the next child record. We start with usWhatRec set to
        // CMA_FIRSTCHILD, then use CMA_NEXT after getting the first child.
        // This enumerates the children of pciParent. The docs don't
        // go into it, but my tests show that using CMA_NEXT after a
        // CMA_FIRSTCHILD will return NULL after the last *child* has been
        // enumerated, rather than the last record in the container. Of course
        // that is how it should work but it's not documented.

        pciNext = WinSendMsg( hwndCnrShare, CM_QUERYRECORD, MPFROMP( pciPrev ),
                              MPFROM2SHORT( usWhatRec, CMA_ITEMORDER ) );

        if( (INT) pciNext == -1 )
        {
            Msg( "RecurseSharedDirs CM_QUERYRECORD RC(%X)", HABERR( hab ) );

            break;
        }

        if( !pciNext )
            break;

        if( (pciNext->attrFile & FILE_DIRECTORY) &&
             pciNext->szFileName[0] != '.' )
            InsertSharedDir( hab, hwndCnrShare, hwndCnr, pciNext, pciNext );

        usWhatRec = CMA_NEXT;

        pciPrev = pciNext;
    }
}

/**********************************************************************/
/*------------------------ InsertSharedRecs --------------------------*/
/*                                                                    */
/*  INSERT SHARED RECORDS FROM OLD CONTAINER TO NEW CONTAINER.        */
/*                                                                    */
/*  INPUT: thread's anchor block handle,                              */
/*         container window handle containing shared records,         */
/*         new container window handle,                               */
/*         parent record in the container with the shared records,    */
/*         parent record in new container                             */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: TRUE or FALSE if successful or not                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static BOOL InsertSharedRecs( HAB hab, HWND hwndCnrShare, HWND hwndCnr,
                              PCNRITEM pciShrParent, PCNRITEM pciParent )
{
    BOOL         fSuccess = TRUE;
    USHORT       usWhatRec = (pciShrParent==NULL) ? CMA_FIRST : CMA_FIRSTCHILD;
    PCNRITEM     pciPrev = pciShrParent, pciNext;
    RECORDINSERT ri;

    // We insert the shared records one record at a time because the container
    // seems to be very touchy if records are inserted in 100-record blocks,
    // then sorted in the original container. If after they are sorted in a
    // different way that they were inserted into the container and we try and
    // insert them as shared records in another container we get strange errors
    // on the insert. So we remain safe and insert one at a time. Because of
    // that we don't invalidate each record on the insert if we are at the top
    // level because the user would see that. Instead we will invalidate all
    // top level records at once in the function that called us. If we are not
    // on the top level, we invalidate as we go. This affects performance a bit
    // but at least the user doesn't really notice it.

    (void) memset( &ri, 0, sizeof( RECORDINSERT ) );

    ri.cb                 = sizeof( RECORDINSERT );
    ri.pRecordOrder       = (PRECORDCORE) CMA_END;
    ri.pRecordParent      = (PRECORDCORE) pciParent;
    ri.zOrder             = (USHORT) CMA_TOP;
    ri.cRecordsInsert     = 1;
    ri.fInvalidateRecord  = pciParent ? TRUE : FALSE;

    for( ; ; )
    {
        // Get the next child record. We start with usWhatRec set to
        // CMA_FIRSTCHILD, then use CMA_NEXT after getting the first child.
        // This enumerates the children of pciParent. The docs don't
        // go into it, but my tests show that using CMA_NEXT after a
        // CMA_FIRSTCHILD will return NULL after the last *child* has been
        // enumerated, rather than the last record in the container. Of course
        // that is how it should work but it's not documented.

        pciNext = WinSendMsg( hwndCnrShare, CM_QUERYRECORD, MPFROMP( pciPrev ),
                              MPFROM2SHORT( usWhatRec, CMA_ITEMORDER ) );

        if( (INT) pciNext == -1 )
        {
            Msg( "InsertSharedRecs CM_QUERYRECORD RC(%X)", HABERR( hab ) );

            fSuccess = FALSE;

            break;
        }

        if( !pciNext )
            break;

        if( !WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP( pciNext ),
                         MPFROMP( &ri ) ) )
        {
            Msg( "InsertSharedRecs CM_INSERTRECORD for %s RC(%X)",
                 pciNext ? pciNext->szFileName : "", HABERR( hab ) );

            fSuccess = FALSE;

            break;
        }

        usWhatRec = CMA_NEXT;

        pciPrev = pciNext;
    }

    return fSuccess;
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
