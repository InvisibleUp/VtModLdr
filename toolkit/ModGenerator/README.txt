ModGenerator - Generates a mod .ZIP from source C/ASM files and description information.

This is the program you want to run for anything but the most trivial of mods.

# Library requirements:
- jansson
- libarchive
- bfd (from GNU binutils)

Due to bfd requirement, ModGenerator is released under the GPL version 3.
Also, I've only tested compiling this on Linux. Windows users, do what you need to do to compile.

# To run
    - Compile ModGenerator. This is just a binary.
    - Run "ModGenerator <source dir> <output ZIP>". If sucessful, the mod described at <source dir> will be compiled and placed into <output ZIP>.
    
# Description of a mod

    /
    |-- metadata.json: Name, author, etc.
    |-- imports.json: List of dependencies
    |-- variables.json: List of variables mod provides.
    |-- functions.json: List of functions mod provides.
    |-- patches.json: List of manually created patches mod provides
    |-- src/
        | -- *.c: Various C source files
        | -- *.asm: Various x86 ASM source files
    |-- data/
        |-- *: Various binary data files
        
## metadata.json
Follows same format as root of output `info.json`, excluding array keys. See mod loader manual for details.
Root key is an object.

## imports.json
Is identical to `dependencies` node of `info.json` of output, with extra information.
Array of objects with following keys:

`Name`: Friendly name of mod used when mod is not found during install
`Auth`: Friendly author of mod used when mod is not found during install
`UUID`: UUID of mod required
`Ver`: Minimum version of mod required
`Variables`: Optional array containing the following
    `UUID`: UUID of variable required from mod
    `Type`: Type of variable. See mod loader manual for details.
    `Update`: (Optional) Expression to increment variable by

## variables.json
Array of objects with following keys:

`UUID`: UUID of variable
`Type`: Type of variable. See mod loader manual for details
`Default`: Default value of variable
`Desc`: Description of variable
`PublicType`: Display type of variable in config GUI. See mod loader manual for details

## functions.json
Array of objects with following keys:

`Name`: Name of function to compile.
`File`: File path to patch
`Start`: Start location of new patches. Default "0".
`End`: End location of new patches. Default "0x7FFFFFFF".
`Repl`: 1 if this replaces an existing chunk of code, 0 otherwise.
`Condition`: Variable condition to determine whether or not function should be installed
`Src`: Object. Each element is the location of source file relative to `/src/` directory. Only one will be used, picked in order presented if said compiler is present on machine.
    `cc`: C source file for GCC and compatibles
    `msvc`: C source file for MSVC (cl.exe)
    `as`: x86 ASM file for GNU AS and compatibles
    `ml`: x86 ASM file for Microsoft's MASM (ml.exe)
    
## patches.json
Is identical to `patches` node of `info.json` of output. See mod loader manual for details.
Applied before function patches are generated.

## patches-post.json
Is identical to `patches` node of `info.json` of output. See mod loader manual for details.
Applied after function patches are generated.

## src/*.c
C files. One function per file.

## src/*.asm
x86 ASM files. One function per file.

## data/*
Various data file. These will be directly copied into the ZIP.

## C Tips & Tricks!
Just due to how the code is compiled, some things are rather strange. Here's some things to watch out for:

- Declare all your external variables as `extern` at the top of your file.
- One funtion per file!
- To use an extern int, you may need to use the & (Address of) prefix.
