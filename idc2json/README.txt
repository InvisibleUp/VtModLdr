Part of the Mod Creation Toolkit

This self-contained thingy converts an IDA 5.0 Free .IDC dump into a game description JSON. Easy peasy support for new games!

To use, export a "database .IDC" from IDA and place it in the same directory as "idc2json.c".
Then, after making the changes listed below, run 
    "cc idc2json.c <dump.c> -ljansson -o <output file>" 
and run the resulting binary. Compilation may take a little while.

Changes needed to be made to dump IDC for use:
    - Change extension to ".c"
    - Change '#include <idc.idc>' to '#include "idc.h"'
    - Chenge 'static main(void) {' to 'int idc_main(void) {'
