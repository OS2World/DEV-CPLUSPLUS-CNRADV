/*********************************************************************
 *                                                                   *
 * MODULE NAME :  cnradv.h               AUTHOR:  Rick Fishman       *
 * DATE WRITTEN:  11-30-92                                           *
 *                                                                   *
 * DESCRIPTION:                                                      *
 *                                                                   *
 *  Common definitions and function prototypes for CNRADV.EXE        *
 *                                                                   *
 * HISTORY:                                                          *
 *                                                                   *
 *  11-30-92 - Source copied from CNRMENU.EXE sample.                *
 *             Added SelectionType submenu ids.                      *
 *             Added new View submenu ids.                           *
 *             Added fMiniIcons to INSTANCE structure.               *
 *             Added ID_ICON_FOLDER definition for def folder icon.  *
 *             Added global variable hptrFolder.                     *
 *  12-05-92   Added drag.c function prototypes/definitions.         *
 *             Added definition of ID_BMP_BACKGROUND.                *
 *             Added pfnwpDefCnr to INSTANCE structure.              *
 *             Added draw.c function prototypes/definitions.         *
 *             Added Background submenu and item ids.                *
 *             Added clrBackground to INSTANCE structure.            *
 *             Added global variable hbmBackground.                  *
 *             Added flCurrentView to INSTANCE structure.            *
 *                                                                   *
 *  Rick Fishman                                                     *
 *  Code Blazers, Inc.                                               *
 *  4113 Apricot                                                     *
 *  Irvine, CA. 92720                                                *
 *  CIS ID: 72251,750                                                *
 *                                                                   *
 *********************************************************************/

/*********************************************************************/
/*------------------- APPLICATION DEFINITIONS -----------------------*/
/*********************************************************************/

#define ID_FIRST_DIRWINDOW   1         // Id of first Directory frame window

#define ID_RESOURCES         1         // Id of frame wnd resources in .RC file

#define CNR_DIRECTORY        30        // The start of our show

#define ID_ICON_FOLDER       100       // Default folder icon

#define ID_BMP_BACKGROUND    200       // Container background bitmap

#define ID_CONTEXT_MENU      1000      // Id of the context menu
#define IDM_VIEW_SUBMENU     1100      // View Submenu id
#define IDM_VIEW_ICON        1101      // View Submenu item ids
#define IDM_VIEW_NAME        1102
#define IDM_VIEW_NAMEFLOWED  1103
#define IDM_VIEW_TREEICON    1104
#define IDM_VIEW_TREENAME    1105
#define IDM_VIEW_TREETEXT    1106
#define IDM_VIEW_DETAILS     1107
#define IDM_VIEW_TEXT        1108
#define IDM_VIEW_TEXTFLOWED  1109
#define IDM_VIEW_MINI        1110
#define IDM_SORT_SUBMENU     1200      // Sort Submenu id
#define IDM_SORT_NAME        1201      // Sort Submenu item ids
#define IDM_SORT_DATETIME    1202
#define IDM_SORT_DIRORDER    1203
#define IDM_OTHERWIN_SUBMENU 1300      // "Other Window" Submenu
#define IDM_OTHERWIN_ITEM1   1301      // ID of first "Other Window" menu item
#define IDM_OTHERWIN_LASTITEM 1399     // ID of last "Other Window" menu item
#define IDM_CREATE_NEWWIN    1400      // CreateNewWindow menu item
#define IDM_ARRANGE          1500      // Arrange menu item
#define IDM_SEL_TYPE         1600      // Selection Type submenu
#define IDM_SEL_SINGLE       1601      // Selection type item ids
#define IDM_SEL_EXTENDED     1602
#define IDM_SEL_MULTIPLE     1603
#define IDM_BKGD_TYPE        1700      // Background Type submenu
#define IDM_BKGD_RED         1701      // Background type item ids
#define IDM_BKGD_WHITE       1702
#define IDM_BKGD_YELLOW      1703
#define IDM_BKGD_BLUE        1704
#define IDM_BKGD_GREY        1705
#define IDM_BKGD_BITMAP      1706

#define ICON_COLUMN          1         // DetailsView column identifiers
#define FILENAME_COLUMN      2         //   (these are placed in the
#define FILESIZE_COLUMN      3         //    pUserData field of the FIELDINFO
#define DATE_COLUMN          4         //    struct and are used by this
#define TIME_COLUMN          5         //    application to identify which
#define FILEATTR_COLUMN      6         //    column we are in during ownerdraw)

#define UM_CONTAINER_FILLED  WM_USER   // Posted by Fill thread to primary thrd

#define DEBUG_FILENAME       "cnradv.dbg"

#define DIRECTORY_WINCLASS   "CnrAdvancedSample"

#define PROGRAM_TITLE        "CNRADV"

#define HABERR( hab )        (ERRORIDERROR( WinGetLastError( hab ) ))

#define ANCHOR( hwnd )       (WinQueryAnchorBlock( hwnd ))

#define HWNDERR( hwnd )       \
            (ERRORIDERROR( WinGetLastError( WinQueryAnchorBlock( hwnd ) ) ))

#define INSTDATA( hwnd )      (WinQueryWindowPtr( hwnd, 0 ))

#define PARENT( hwnd )        (WinQueryWindow( hwnd, QW_PARENT ))

#define OWNER( hwnd )         (WinQueryWindow( hwnd, QW_OWNER ))

#define GRANDPARENT( hwnd )   (WinQueryWindow( WinQueryWindow(hwnd, QW_PARENT)))

#ifndef CRA_SOURCE            // As of 10/30/92, CRA_SOURCE not in toolkit hdrs
#  define CRA_SOURCE          0x00004000L
#endif

/**********************************************************************/
/*---------------------------- STRUCTURES ----------------------------*/
/**********************************************************************/

typedef struct _CNRITEM               // CONTAINER RECORD STRUCTURE
{
  MINIRECORDCORE rc;                  // Base information
  PSZ            pszFileName;         // Pointer to File name
  HPOINTER       hptrIcon;            // Icon handle
  CDATE          date;                // Date of file
  CTIME          time;                // Time of file
  ULONG          cbFile;              // File size
  ULONG          attrFile;            // Attributes of this file
  INT            iDirPosition;        // Relative position in directory
  BOOL           fSelected;           // Does this record have source emphasis
  CHAR           szFileName[CCHMAXPATH + 1]; // File name

} CNRITEM, *PCNRITEM;

#define EXTRA_RECORD_BYTES (sizeof( CNRITEM ) - sizeof( MINIRECORDCORE ))


typedef struct _INSTANCE              // CONTAINER'S CLIENT WINDOW INSTANCE DATA
{
    BOOL fShutdown;                     // Main thread wants to shut down
    BOOL fContainerFilled;              // Fill thread has completed
    PCNRITEM pciSelected;               // Item selected under context menu
    BOOL fDirSelected;                  // One of the selected recs is a dir
    BOOL fMiniIcons;                    // Are we using mini icons?
    PFNWP pfnwpDefCnr;                  // Default container window procedure
    COLOR clrBackground;                // Cnr background color (0 for bitmap)
    ULONG flCurrentView;                // Current major view type (CV_?)

    // Used to keep track of frame windows associated with OtherWindow menuitems
    HWND hwndFrame[ IDM_OTHERWIN_LASTITEM - IDM_OTHERWIN_ITEM1 + 1 ];

    CHAR szCnrTitle[ CCHMAXPATH + 20 ]; // Container title
    CHAR szDirectory[ CCHMAXPATH + 1 ]; // Directory being displayed
    CHAR achWorkBuf[ CCHMAXPATH + 1 ];  // Instance work buffer

} INSTANCE, *PINSTANCE;


typedef struct _THREADPARMS          // PARAMETERS TO PASS TO SECONDARY THREAD
{
    HWND hwndClient;                 // Client window handle
    HWND hwndCnrShare;               // Container hwnd whose recs will be shared
    PCNRITEM pciParent;              // Ptr to parent CNRITEM if using shr recs

} THREADPARMS, *PTHREADPARMS;


typedef struct _WINCREATE            // PARAMETERS TO PASS TO WINDOW ON CREATION
{
    PSZ szDirectory;                 // Pointer to directory name
    HWND hwndCnrShare;               // Container hwnd whose recs will be shared
    PCNRITEM pciParent;              // Ptr to parent CNRITEM if using shr recs

} WINCREATE, *PWINCREATE;

/**********************************************************************/
/*----------------------- FUNCTION PROTOTYPES ------------------------*/
/**********************************************************************/

// In common.c

VOID SetWindowTitle( HWND hwndClient, PSZ szFormat, ... );
VOID Msg( PSZ szFormat, ... );
VOID FullyQualify( PSZ szDirectory, HWND hwndCnr, PCNRITEM pci );

// In create.c

HWND CreateDirectoryWin( PSZ szDirectory, HWND hwndCnr, PCNRITEM pciFirst );
HWND CreateContainer( HWND hwndClient, PSZ szDirectory, HWND hwndCnrShare,
                      PCNRITEM pciFirst );

// In populate.c

VOID PopulateContainer( PVOID pThreadParms );

// In ctxtmenu.c

VOID CtxtmenuCreate( HWND hwndClient, PCNRITEM pciSelected );
VOID CtxtmenuCommand( HWND hwndClient, ULONG idCommand, ULONG ulCmdSrc );
VOID CtxtmenuSetView( HWND hwndClient, ULONG ulViewType );
VOID CtxtmenuEnd( HWND hwndClient );

// In edit.c

VOID EditBegin( HWND hwndClient, PCNREDITDATA pced );
VOID EditEnd( HWND hwndClient, PCNREDITDATA pced );

// In sort.c

VOID SortContainer( HWND hwndClient, ULONG ulSortType );

// In drag.c

MRESULT DragMessage( HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2 );
VOID    DragInit( HWND hwndClient, PCNRDRAGINIT pcdi );
MRESULT DragOver( HWND hwndClient, PCNRDRAGINFO pcdi );
VOID    DragDrop( HWND hwndClient, PCNRDRAGINFO pcdi );

// In draw.c

VOID    DrawSubclassCnr( HWND hwndClient );
MRESULT DrawItem( HWND hwndClient, POWNERITEM poi );

/**********************************************************************/
/*------------------------ GLOBAL VARIABLES --------------------------*/
/**********************************************************************/

#ifdef GLOBALS_DEFINED
#  define DATADEF
#else
#  define DATADEF extern
#endif

DATADEF INT iWinCount;           // Number of directory windows created
DATADEF HPOINTER hptrFolder;     // Folder Icon handle
DATADEF HBITMAP hbmBackground;   // Bitmap to display on container background

/***********************************************************************
 *                   E N D     O F     S O U R C E                     *
 **********************************************************************/
