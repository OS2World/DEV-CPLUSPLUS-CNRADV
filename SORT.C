/*********************************************************************
 *                                                                   *
 * MODULE NAME :  sort.c                 AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  This module is part of CNRADV.EXE. It contains the functions    *
 *  necessary to implement container sorting.                        *
 *                                                                   *
 * CALLABLE FUNCTIONS:                                               *
 *                                                                   *
 *  VOID SortContainer( HWND hwndClient, ULONG ulSortType );         *
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

#define INCL_WINERRORS
#define INCL_WINFRAMEMGR
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINWINDOWMGR

/**********************************************************************/
/*----------------------------- INCLUDES -----------------------------*/
/**********************************************************************/

#include <os2.h>
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

static SHORT APIENTRY NameCompare( PRECORDCORE prc1,PRECORDCORE prc2,PVOID pv );
static SHORT APIENTRY DirCompare ( PRECORDCORE prc1,PRECORDCORE prc2,PVOID pv );
static SHORT APIENTRY DateCompare( PRECORDCORE prc1,PRECORDCORE prc2,PVOID pv );

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

/**********************************************************************/
/*-------------------------- SortContainer ---------------------------*/
/*                                                                    */
/*  SORT THE CONTAINER BY A KEY.                                      */
/*                                                                    */
/*  INPUT: client window handle,                                      */
/*         type of sort                                               */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: nothing                                                   */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
VOID SortContainer( HWND hwndClient, ULONG ulSortType )
{
    HWND hwndCnr = WinWindowFromID( hwndClient, CNR_DIRECTORY );

    switch( ulSortType )
    {
        case IDM_SORT_NAME:

            WinSendMsg( hwndCnr, CM_SORTRECORD, MPFROMP( NameCompare ), NULL );

            break;

        case IDM_SORT_DIRORDER:

            WinSendMsg( hwndCnr, CM_SORTRECORD, MPFROMP( DirCompare ), NULL );

            break;

        case IDM_SORT_DATETIME:

            WinSendMsg( hwndCnr, CM_SORTRECORD, MPFROMP( DateCompare ), NULL );

            break;
    }
}

/**********************************************************************/
/*--------------------------- NameCompare ----------------------------*/
/*                                                                    */
/*  COMPARISON FUNCTION FOR FILENAME SORT.                            */
/*                                                                    */
/*  INPUT: first record in the compare,                               */
/*         second record in the sort,                                 */
/*         dummy parm to satisfy function prototype                   */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: 0 = both are equal                                        */
/*         -1 = first is less than second                             */
/*         +1 = first is greater than second                          */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static SHORT APIENTRY NameCompare( PRECORDCORE prc1, PRECORDCORE prc2, PVOID pv)
{
    pv = pv;    // to keep the compiler happy

    return strcmp( ((PCNRITEM)prc1)->szFileName, ((PCNRITEM)prc2)->szFileName );
}

/**********************************************************************/
/*---------------------------- DirCompare ----------------------------*/
/*                                                                    */
/*  COMPARISON FUNCTION FOR DIRECTORY ORDER SORT.                     */
/*                                                                    */
/*  INPUT: first record in the compare,                               */
/*         second record in the sort,                                 */
/*         dummy parm to satisfy function prototype                   */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: 0 = both are equal                                        */
/*         -1 = first is less than second                             */
/*         +1 = first is greater than second                          */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static SHORT APIENTRY DirCompare( PRECORDCORE prc1, PRECORDCORE prc2, PVOID pv )
{
    INT iDirPosition1 = ((PCNRITEM) prc1)->iDirPosition;
    INT iDirPosition2 = ((PCNRITEM) prc2)->iDirPosition;

    pv = pv;    // to keep the compiler happy

    if( iDirPosition1 == iDirPosition2 )
        return 0;
    else if( iDirPosition1 < iDirPosition2 )
        return -1;
    else
        return +1;
}

/**********************************************************************/
/*--------------------------- DateCompare ----------------------------*/
/*                                                                    */
/*  COMPARISON FUNCTION FOR FILE DATE/TIME SORT.                      */
/*                                                                    */
/*  INPUT: first record in the compare,                               */
/*         second record in the sort,                                 */
/*         dummy parm to satisfy function prototype                   */
/*                                                                    */
/*  1.                                                                */
/*                                                                    */
/*  OUTPUT: 0 = both are equal                                        */
/*         -1 = first is less than second                             */
/*         +1 = first is greater than second                          */
/*                                                                    */
/*--------------------------------------------------------------------*/
/**********************************************************************/
static SHORT APIENTRY DateCompare( PRECORDCORE prc1, PRECORDCORE prc2, PVOID pv)
{
    CDATE date1 = ((PCNRITEM) prc1)->date;
    CDATE date2 = ((PCNRITEM) prc2)->date;
    CHAR  szDate1[ 9 ], szDate2[ 9 ];
    INT   iResult;

    pv = pv;    // to keep the compiler happy

    (void) sprintf( szDate1,"%04u%02u%02u",date1.year, date1.month, date1.day );
    (void) sprintf( szDate2,"%04u%02u%02u",date2.year, date2.month, date2.day );

    iResult = strcmp( szDate1, szDate2 );

    if( !iResult )
    {
        CTIME time1 = ((PCNRITEM) prc1)->time;
        CTIME time2 = ((PCNRITEM) prc2)->time;
        INT iSecs1 = (INT)((time1.hours*3600)+(time1.minutes*60)+time1.seconds);
        INT iSecs2 = (INT)((time2.hours*3600)+(time2.minutes*60)+time2.seconds);

        if( iSecs1 == iSecs2 )
            iResult = 0;
        else if( iSecs1 < iSecs2 )
            iResult = -1;
        else
            iResult = +1;
    }

    return iResult;
}

/*************************************************************************
 *                     E N D     O F     S O U R C E                     *
 *************************************************************************/
