idc2whitelist

This self-contained C file converts an IDA 5.0 Free .IDC dump (which actually is a C file) into a whitelist for use in a game description JSON. Easy peasy support for new games!

To use, export a "database .IDC" from IDA and place it in the same directory as "idc2whitelist.c".
To that file, do the following:
    - Change extension to ".c"
    - Change '#include <idc.idc>' to '#include "idc.h"'
    - Change 'static main(void) {' to 'int idc_main(void) {'
Then run 
    "cc idc2json.c <dump.c> -ljansson -o <output file>" 
or "cl" on systems using Visual Studio.

Run the resulting binary and it will create a whitelist at "whitelist.json". Copy and paste this into your game description JSON.

