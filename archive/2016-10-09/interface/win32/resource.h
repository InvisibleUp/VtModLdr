//Borland C++ 5.5/Visual Studio 6.0 Compatibility Hacks
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE 64
#endif

#ifndef TVS_CHECKBOXES
#define TVS_CHECKBOXES 256
#endif

#ifndef BIF_USENEWUI
#define BIF_USENEWUI BIF_NEWDIALOGSTYLE | BIF_EDITBOX
#endif

/*Resource file IDs
  Format: TGGii
  T: Type
    0: System required (icons, manifests, etc.)
    1: Dialogs
    2: Menus
    3: Bitmaps
    4: Controls
    5-9: ???
  G: Group
  i: Item
*/

#define IDI_ICON                                00010
#define IDB_ABOUTIMG                            30000

#define IDD_MAIN                                10000
#define     IDLOAD                              40002
#define     IDREMOVE                            40001
#define     IDC_MAINNAME                        40003
#define     IDC_MAINAUTHOR                      40004
#define     IDC_MAINVERSION                     40005
#define     IDC_MAINEXESPACE                    40006
#define     IDC_MAINDESC                        40007
#define     IDC_MAINDISKSPACE                   40008
#define     IDC_MAINPREVIEW                     40009
#define     IDC_MAINMODLIST                     40010
#define     IDC_MAINDISKMORE                    40011
#define     IDC_MAINTYPE                        40012
#define     IDC_MAINEXEMORE                     40013
#define     IDC_MAINVARS                        40014       
#define     IDC_MAINRUN                         40015          
#define IDR_MAIN                                20000
#define     IDO_COMPO                           40015
#define     IDO_ABOUT                           40016
#define     IDO_CHANGEPROF                      40017

#define IDD_ABOUT                               10001
#define     IDC_ABOUTCOPYTEXT                   40100
#define     IDC_ABOUTIMAGE                      40101

#define IDD_DISK                                10002
#define     IDC_DISKINFO                        40200
#define     IDC_DISKLIST                        40201
#define     IDDISKPATCHES                       40202

#define IDD_PATCHES                             10003
#define     IDC_PATCHHEX                        40300
#define     IDC_PATCHPRE                        40301
#define     IDC_PATCHPOST                       40302
#define     IDC_PATCHLIST                       40303
#define     IDC_PATCHASCII                      40304

#define IDD_PROGRESS                            10004
#define     IDC_PROGBAR                         40400
#define     IDC_PROGLABEL                       40401

#define IDD_PROGCONFIG                          10005
#define     IDC_CFGPROFPATH                     40500
#define     IDC_CFGPROFNEW                      40501
#define     IDC_CFGPROFBROWSE                   40502
#define     IDC_CFGGAMEPATH                     40510
#define     IDC_CFGGAMEBROWSE                   40511
#define     IDC_CFGGAMENAME                     40513
#define     IDC_CFGGAMEVER                      40514
#define     IDC_CFGRUNPATH                      40520
#define     IDC_CFGMETAPATH                     40530
