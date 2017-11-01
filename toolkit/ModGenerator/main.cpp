// SrModLdr Generator v1.0
// Licensed under GPL v3

// And yeah, this is a slapped together hack
// with not much error handling or anything

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <stdint.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <bfd.h>
#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "jansson.h"
#include "functproto.hpp"

// Converts type-generic number to std::string
// https://stackoverflow.com/a/5590404
#define CppNumToStr( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()
        
// Copies a directory from `source` to `destination`
// https://stackoverflow.com/a/8594696
bool CopyDir(
    boost::filesystem::path const & source,
    boost::filesystem::path const & destination
) {
    namespace fs = boost::filesystem;
    try {
        // Check whether the function call is valid
        if (
            !fs::exists(source) ||
            !fs::is_directory(source)
        ) {
            std::cerr << "Source directory " << source.string()
                << " does not exist or is not a directory." << '\n';
            return false;
        }
        
        if (fs::exists(destination)) {
            std::cerr << "Destination directory " << destination.string()
                << " already exists." << '\n';
            return false;
        }
        // Create the destination directory
        if (!fs::create_directory(destination))
        {
            std::cerr << "Unable to create destination directory"
                << destination.string() << '\n';
            return false;
        }
    } catch(fs::filesystem_error const & e) {
        std::cerr << e.what() << '\n';
        return false;
    }
    
    // Iterate through the source directory
    for(
        fs::directory_iterator file(source);
        file != fs::directory_iterator(); file++
    ) {
        try {
            fs::path current(file->path());
            if(fs::is_directory(current)) {
                // Found directory: Recursion
                if (!CopyDir(current, destination / current.filename())) {
                    return false;
                }
            } else {
                // Found file: Copy
                fs::copy_file(current, destination / current.filename() );
            }
        } catch(fs::filesystem_error const & e) {
            std:: cerr << e.what() << '\n';
        }
    }
    return true;
}

// Define classes for every type of thing we have
class Compiler {
private:
    bool prog_exists(const char *progname) {
        std::string command = progname;
        command += " --version";
        return (system(command.c_str()) == 0);
    }
    
public:
    enum CCVendor {CC, MSVC};
    enum ASVendor {AS, ML};
    CCVendor cc_vendor;
    ASVendor as_vendor;
    std::string Path;
    
    Compiler() {
        // empty
    }
    
    Compiler(std::string path) {
        Path = path;
        
        // Find compiler type/path
        if(prog_exists("cc")){
            cc_vendor = CC; // GCC-like
        } else if(prog_exists("cl")){
            cc_vendor = MSVC; // MSVC
        } else {
            std::cerr << "No C compiler found in system path!\n";
            throw(1);
        }
        
        // Find assembler type/path
        if(prog_exists("as")){
            as_vendor = AS; // GNU-like
        } else if(prog_exists("ml")){
            as_vendor = ML; // MASM-like
        } else {
            std::cerr << "No x86 assembler found in system path!\n";
            throw(1);
        }
        
        // Setup directories
        std::string binpath = path;
        binpath += "/out/";
        boost::filesystem::remove_all(binpath);
        boost::filesystem::create_directory(binpath);
        binpath += "bin/";
        boost::filesystem::create_directory(binpath);
        
        std::cout << "\n";
    }
    
    void compile(const char *srcfile, const char *outfile) {
        switch(cc_vendor){
        case CC: {
            std::string command = "cc ";
            command += srcfile;
            command += " -c -masm=intel -m32 -march=i486 -Os -I ./include/ -o ";
            command += outfile;
            
            system(command.c_str());
            break;
        }
        
        case MSVC: {
            std::cerr << "MSVC not currently supported. (Sorry!)\n";
            throw(1);
            break;
        }
        }
    }
    
    void assemble(const char *srcfile, const char *outfile) {
        switch(cc_vendor){
        case CC: {
            std::string command = "as --32 -R ";
            command += srcfile;
            command += " -o ";
            command += outfile;
            system(command.c_str());
            break;
        }
        
        case MSVC: {
            std::cerr << "MASM not currently supported. (Sorry!)\n";
            throw(1);
            break;
        }
        }
    }
};

class Metadata {
    
private:
    void constructor(json_t *entry) {
        uuid = JSON_Value_String(entry, "UUID");
        name = JSON_Value_String(entry, "Name");
        desc = JSON_Value_String(entry, "Info");
        auth = JSON_Value_String(entry, "Author");
        ver = JSON_Value_String(entry, "Version");
        date = JSON_Value_String(entry, "Date");
        cat = JSON_Value_String(entry, "Category");
        ml_ver = JSON_Value_String(entry, "ML_Ver");
        game = JSON_Value_String(entry, "Game");
    }
    
public:
    std::string uuid;
    std::string name;
    std::string desc;
    std::string auth;
    std::string ver;
    std::string date;
    std::string cat;
    std::string ml_ver;
    std::string game;
    json_t *json;
    
    Metadata() {
        //nothing
    }
    
    Metadata(json_t *entry) {
        constructor(entry);
    }
    
    Metadata(std::string path) {
        json = JSON_Load(path.c_str());
        if(!json){ 
            std::cerr << "metadata.json not found!\n";
            throw (1); 
        }
        
        constructor(json);
    }
    
    
};

class Variable {
public:
    enum ModeEnum {NEW, UPDATE};
    std::string UUID;
    std::string Type;
    std::string Value;
    std::string Desc;
    std::string PublicType;
    ModeEnum Mode;
    
    Variable() {
        //empty
    }
    
    Variable(json_t *entry) {
        UUID = JSON_Value_String(entry, "UUID");
        Type = JSON_Value_String(entry, "Type");
        Desc = JSON_Value_String(entry, "Desc");
        PublicType = JSON_Value_String(entry, "PublicType");
        
        Value = JSON_Value_String(entry, "Default");
        if(!Value.empty()){
            Mode = NEW;
        } else {
            // Actually uses Update
            Value = JSON_Value_String(entry, "Update");
            Mode = UPDATE;
        }
    }
};

class Import {
public:
    std::string Name;
    std::string UUID;
    std::string Auth;
    std::string Ver;
    std::vector<Variable> Variables;
    
    Import() {
        // empty
    }
    
    Import(json_t *entry) {
        Name = JSON_Value_String(entry, "Name");
        UUID = JSON_Value_String(entry, "UUID");
        Auth = JSON_Value_String(entry, "Auth");
        Ver  = JSON_Value_String(entry, "Ver");
        
        size_t i;
        json_t *out, *row;
        
        out = json_object_get(entry, "Variables");
        if(!out){ return; }
        
        json_array_foreach(out, i, row){
            Variables.push_back(Variable(row));
        }
    }
    
};

class Function {
public: 
    struct relocinfo {
        std::string section;
        std::string symbol;
        unsigned long pos;
    };
    bfd *object;
    
private:
    std::vector<relocinfo> relocs;
    std::vector<std::string> symbols;
    std::vector<std::string> sections;
    Compiler _compiler;
    asymbol **symbol_table;
    unsigned long symbol_count;
    
public:
    enum Type {C, ASM};
    Type type;
    std::string name;
    std::string src;
    std::string objpath_abs;
    std::string objpath;
    std::string destpath;
    std::string cond;
    unsigned long repl;
    std::string start;
    std::string end;
    
    Function(json_t *entry, Compiler compiler){
        _compiler = compiler;
        symbol_table = NULL;
        object = NULL;
        
        name = JSON_Value_String(entry, "Name");
        src = compiler.Path;
        src += "/src/";
        
        repl = JSON_Value_uLong(entry, "Repl");
        destpath = JSON_Value_String(entry, "File");
        start = JSON_Value_String(entry, "Start");
        end = JSON_Value_String(entry, "End");
        cond = JSON_Value_String(entry, "Condition");
        
        if(start == ""){
            start = "0";
        }
        
        if(end == ""){
            // Automatically set it to largest addressable size, 2^31 - 1
            end = "0x7FFFFFFF";
        }
        
        json_t *files = json_object_get(entry, "Src");
        if(!files){
            std::cerr << "Src key not found in an entry in functions.json";
            throw(1); 
        }
        
        if(compiler.cc_vendor == Compiler::CC) {
            std::string file = JSON_Value_String(files, "cc");
            if(!file.empty()){
                src += file;
                type = C;
                return;
            }
        } if (compiler.cc_vendor == Compiler::MSVC) {
            std::string file = JSON_Value_String(files, "msvc");
            if(!file.empty()){
                src += file;
                type = C;
                return;
            }
        } if (compiler.as_vendor == Compiler::AS) {
            std::string file = JSON_Value_String(files, "as");
            if(!file.empty()){
                src += file;
                type = ASM;
                return;
            }
        } if (compiler.as_vendor == Compiler::ML) {
            std::string file = JSON_Value_String(files, "ml");
            if(!file.empty()){
                src += file;
                type = ASM;
                return;
            }
        }
        
        std::cerr << "No source files matching any compilers/assemblers installed on system\n";
        throw(1);
    }
    
    void create_obj(){
        // Return if we already did this
        if(!objpath_abs.empty()){
            return;
        }
        
        objpath_abs = _compiler.Path;
        objpath_abs += "/out/bin/";
        objpath_abs += name;
        objpath_abs += ".o";
        
        objpath = "bin/";
        objpath += name;
        objpath += ".o";
        
        
        switch(type) {
        case C:
            _compiler.compile(src.c_str(), objpath_abs.c_str());
        break;
        
        case ASM:
            _compiler.assemble(src.c_str(), objpath_abs.c_str());
        break;
        }
        
        // Open via BFD
        object = bfd_openr(objpath_abs.c_str(), NULL);
        if(object == NULL){
            std::cerr << "Error initializing BFD library\n";
            throw(1);
        }
        bfd_check_format(object, bfd_object);
        
    }
    
    asymbol ** get_symbol_table() {
        if (symbol_table == NULL) {
            unsigned long size = bfd_get_symtab_upper_bound(object);
            if(size <= 0){return NULL;} // Possibly problmatic...
        
            symbol_table = (asymbol **)malloc(size);
            symbol_count = bfd_canonicalize_symtab (object, symbol_table);
        }
        
        return symbol_table;
    }
    
    std::vector<relocinfo> get_reloc(){
        if(!relocs.empty()){
            return relocs;
        }
        
        asection *section;
        
        for (section = object->sections; section != NULL; section = section->next){
            arelent **relpp, **p;
            long relcount;
            long relsize;
            
            if (   
                bfd_is_abs_section (section) ||
                bfd_is_und_section (section) ||
                bfd_is_com_section (section) ||
                ((section->flags & SEC_RELOC) == 0)
            ) {
                continue;
            }

            relsize = bfd_get_reloc_upper_bound(object, section);
            if (relsize == 0) {
                continue;
            }
            
            // Get symbol table for "horrible internal magic reasons"
            get_symbol_table();

            // Get relocations table
            relpp = (arelent **)malloc(relsize);
            relcount = bfd_canonicalize_reloc(object, section, relpp, symbol_table);

            for (p = relpp; relcount && *p != NULL; p++, relcount--) {
                relocinfo row;
                
                row.section = std::string(section->name);
                row.symbol = std::string((*(*p)->sym_ptr_ptr)->name);
                row.pos = (*p)->address;
                relocs.push_back(row);
                
                //std::cout << row.section << "\t" << row.symbol << "\t" << row.pos << "\n";
            }
            
            free(relpp);
        }
            
        return relocs;
    }
    
    std::vector<std::string> get_symbol(){
        if(!symbols.empty() || objpath.empty()){
            return symbols;
        }
        
        if (!(bfd_get_file_flags(object) & HAS_SYMS)) {
            return symbols;
        }

        get_symbol_table();
        
        for (long i = 0; i < symbol_count; i++){
            asymbol *sym = symbol_table[i];
            if (
                //sym->name == NULL || sym->name[0] == '\0' ||
                //sym->flags & (BSF_DEBUGGING | BSF_SECTION_SYM) ||
                bfd_is_und_section (sym->section) //|| 
                //bfd_is_com_section (sym->section)
            ){
                symbols.push_back(std::string(symbol_table[i]->name));
                //std::cout << "SYMB: " << symbol_table[i]->name << "\n";
            }
        }
        return symbols;
    }
    
    // Get list of sections
    std::vector<std::string> get_sections(){
        if(!sections.empty() || objpath.empty()){
            return sections;
        }

        asection *text = NULL;
        for (
            asection *section = object->sections;
            section != NULL;
            section = section->next
        ){
            sections.push_back(std::string(section->name));
            //std::cout << "SECT: " << section->name << "\n";
        }
        
        return sections;
    }
    
    // Get starting address of ".text" section in obj file
    unsigned long objstart() {
       
        // Find .text section
        asection *text = NULL;
        for (
            asection *section = object->sections;
            section != NULL;
            section = section->next
        ){
            if (strcmp(section->name, ".text") == 0) {
                text = section;
                break;
            }
        }
        if (text == NULL) { return 0; } // Shouldn't happen
        
        return text->filepos;
    }
    
    // Get ending address of ".text" section in obj file
    unsigned long objend() {
        // Find .text section
        asection *text = NULL;
        for (
            asection *section = object->sections;
            section != NULL;
            section = section->next
        ){
            if (strcmp(section->name, ".text") == 0) {
                text = section;
                break;
            }
        }
        if (text == NULL) { return 0; } // Shouldn't happen
        
        return text->filepos + text->size;
    }
    
    // We are totally leaking memory and I don't care.
    /*~Function(){
        if(object != NULL){
            bfd_close(object);
        }
    }*/
};

class Patch {
public:
    std::string ID, Mode, File, FileType, SrcFile, SrcFileType,
                SrcFileLoc, AddType, Value, Condition,
                Start, End, Len, SrcStart, SrcEnd;
    
    Patch(){
        // Nothing!
    }
    
    Patch(json_t *entry){
        // Just shove everything into the variables
        ID = JSON_Value_String(entry, "ID");
        Mode = JSON_Value_String(entry, "Mode");
        File = JSON_Value_String(entry, "File");
        FileType = JSON_Value_String(entry, "FileType");
        Start = JSON_Value_String(entry, "Start");
        End = JSON_Value_String(entry, "End");
        Len = JSON_Value_String(entry, "Len");
        SrcFile = JSON_Value_String(entry, "SrcFile");
        SrcFileType = JSON_Value_String(entry, "SrcFileType");
        SrcFileLoc = JSON_Value_String(entry, "SrcFileLoc");
        SrcStart = JSON_Value_String(entry, "SrcStart");
        SrcEnd = JSON_Value_String(entry, "SrcEnd");
        AddType = JSON_Value_String(entry, "AddType");
        Value = JSON_Value_String(entry, "Value");
        Condition = JSON_Value_String(entry, "Condition");
    }
    
    Patch(Function funct, std::string UUID){
        ID = funct.name;
        ID += ".";
        ID += UUID;
        Mode = "COPY";
        File = funct.destpath;
        FileType = "PE";
        
        Start = funct.start;
        End = funct.end;
        Len = CppNumToStr(funct.objend() - funct.objstart()); 
        
        SrcFile = funct.objpath;
        SrcStart = CppNumToStr(funct.objstart());
        SrcEnd = CppNumToStr(funct.objend());
        SrcFileLoc = "Mod";
        Condition = funct.cond;
    }
};

class Mod {
public:
    Metadata metadata;
    std::vector<Variable> variables;
    std::vector<Import> imports;
    std::vector<Patch> patches;
    std::vector<Function> functions;
    
private:
    std::vector<Variable> load_variables(std::string path) {
        std::vector<Variable> out;
        size_t i;
        json_t *row;
        
        json_t *root = JSON_Load(path.c_str());
        if(!root){ return out; }
        
        json_array_foreach(root, i, row){
            out.push_back(Variable(row));
        }
        json_decref(root);
        
        return out;
    }
    
    std::vector<Import> load_imports(std::string path) {
        std::vector<Import> out;
        size_t i;
        json_t *row;
        
        json_t *root = JSON_Load(path.c_str());
        if(!root){ return out; }
        
        json_array_foreach(root, i, row){
            out.push_back(Import(row));
        }
        json_decref(root);
        
        return out;
    }
    
    std::vector<Patch> load_patches(std::string path) {
        std::vector<Patch> out;
        size_t i;
        json_t *row;
        
        json_t *root = JSON_Load(path.c_str());
        if(!root){ return out; }
        
        json_array_foreach(root, i, row){
            out.push_back(Patch(row));
        }
        json_decref(root);
        
        return out;
    }
    
    std::vector<Function> load_functions(std::string path, Compiler compiler) {
        std::vector<Function> out;
        size_t i;
        json_t *row;
        
        json_t *root = JSON_Load(path.c_str());
        if(!root){ return out; }
        
        json_array_foreach(root, i, row){
            out.push_back(Function(row, compiler));
        }
        json_decref(root);
        
        return out;
    }
    
    // Function that compiles and converts a function defintion into a vector of patches.
    std::vector<Patch> Function2Patches(Function funct, const std::vector<Patch> patches)
    {
        std::vector<Patch> out = patches;
        
        std::cout << "\n" << funct.name << "\n===\n";
        
        // Compile and/or assemble function
        funct.create_obj();
        
        // Create patch from that
        out.push_back(Patch(funct, metadata.uuid));
        std::string BaseUUID = out.back().ID;
                
        // Add CLEAR space if needed
        if (funct.repl == 1) {
            out.back().Mode = "CLEAR";
            out.back().ID = "";
            out.push_back(Patch(funct, metadata.uuid));
        }
        
        /* Create list of additional (formerly "mini") patches needed */
        // Get reloc and symbol info from output obj
        std::vector<Function::relocinfo> reloc = funct.get_reloc();
        std::vector<std::string> symbol = funct.get_symbol();
        std::vector<std::string> section = funct.get_sections();
        std::vector<bool> section_added = std::vector<bool>(section.size(), false);

        // For each line of reloc info...

        for(int i = 0; i < reloc.size(); i++){
            bool match;
            
            // Prepare patch
            Patch row = out.back();
            row.ID = CppNumToStr(i);
            row.ID += ".";
            row.ID += BaseUUID;
            row.Mode = "REPL";
            row.AddType = "Expression";
            row.Start = "$ Start.";
            row.Start += BaseUUID;
            row.Start += " + ";
            row.Start += CppNumToStr(reloc[i].pos);
            row.End = "$ Start.";
            row.End += BaseUUID;
            row.End += " + ";
            row.End += CppNumToStr(reloc[i].pos + 4);
            //row.Start = CppNumToStr(strtoul(row.Start.c_str(), NULL, 10) + reloc[i].pos);
            //row.End = CppNumToStr(strtoul(row.Start.c_str(), NULL, 10) + 4);
            row.Len = "4";
            
            // If it's a known segment, handle that
            int sectID = -1;
            for (int j = 0; j < section.size(); j++) {
                if(reloc[i].symbol == ".text"){ break; }
                //std::cout << "AA: Testing " << reloc[i].symbol << " vs. " << section[j] << "...";
                if (reloc[i].symbol == section[j]) {
                    //std::cout << " PASS\n";
                    sectID = j;
                    break;
                } else {
                    //std::cout << " FAIL\n";
                }
            }
            if (sectID != -1){
                
                // Super inefficient but eh, laziness
                asection *sectSrc = bfd_get_section_by_name(funct.object, section[sectID].c_str());
                
                // Get old value of dword to replace (this is offset)
                uint32_t offset = 0;
                {
                    // Load file
                    FILE *objfile = fopen(funct.objpath_abs.c_str(), "rb");
                    if(objfile == NULL){
                        std::cerr << "Error opening file " << funct.objpath_abs << "!\n";
                        continue;
                    }
                    
                    // Get seek position
                    int seekpos = reloc[i].pos + bfd_get_section_by_name(funct.object, reloc[i].section.c_str())->filepos;
                    
                    // Seek to offset in file and read into dword
                    fseek(objfile, seekpos, SEEK_SET);
                    fread(&offset, sizeof(offset), 1, objfile);
                    fclose(objfile);
                    
                    // Debug print offst value
                    //printf("Offset: %i\n", offset);
                }
                
                // Add section as a patch if not already done
                if(section_added[sectID] == false){
                    //std::cout << "Adding patch for " << section[sectID] << "...\n";
                    
                    Patch sectPatch = out.back();
                    sectPatch.ID = section[sectID];
                    sectPatch.ID += ".";
                    sectPatch.ID += BaseUUID;
                    sectPatch.Mode = "COPY";
                    sectPatch.AddType = "Expression";
                    sectPatch.Start = "0";
                    sectPatch.End = "0x7FFFFFFF";
                    sectPatch.SrcStart = CppNumToStr(sectSrc->filepos);
                    sectPatch.SrcEnd = CppNumToStr(sectSrc->filepos + sectSrc->size);
                    sectPatch.Len = CppNumToStr(sectSrc->size);
                    out.push_back(sectPatch);
                    
                    section_added[sectID] = true;
                }
                
                // Then add reloc info, as usual
                row.Value = "$ Start.";
                row.Value += reloc[i].symbol;
                row.Value += ".";
                row.Value += BaseUUID;
                row.Value += " + ";
                row.Value += CppNumToStr(offset);
                
                out.push_back(row);
                continue;
            }
            
            // Filter reloc listing so that "symbol" must be a known symbol.
            match = false;
            for (int j = 0; j < symbol.size(); j++) {
                //std::cout << "A: Testing " << reloc[i].symbol << " vs. " << symbol[j] << "...";
                if (reloc[i].symbol == symbol[j]) {
                    //std::cout << " PASS\n";
                    match = true;
                    break;
                } else {
                    //std::cout << " FAIL\n";
                }
            }
            if (!match){
                continue;
            }

            // If the name is a known function:
            match = false;
            for (int j = 0; j < functions.size(); j++) {
                //std::cout << "B: Testing " << reloc[i].symbol << " vs. " << functions[j].name << "...";
                if (reloc[i].symbol == functions[j].name) {
                    //std::cout << "PASS\n";
                    match = true;
                    break;
                } else {
                    //std::cout << "FAIL\n";
                }
            }
            if (match) {
                // Create patch entry where Bytes = "Start.{VARIABLE}.{MODUUID}"
                //std::cout << "Entry for local funct " << reloc[i].symbol << "\n";
                row.Value = "$ Start.";
                row.Value += reloc[i].symbol;
                row.Value += ".";
                row.Value += metadata.uuid;
                
                out.push_back(row);
                continue;
            }

            // If name is a known variable:
            match = false;
            for (int j = 0; j < variables.size(); j++) {
                //std::cout << "C: Testing " << reloc[i].symbol << " vs. " << variables[j].UUID << "...";
                // Hackishly use strncmp to test symbol name against 
                // first part of variable UUID.
                if (strncmp(variables[j].UUID.c_str(), reloc[i].symbol.c_str(), reloc[i].symbol.size()) == 0) {
                    //std::cout << "PASS\n";
                    match = true;
                    break;
                } else {
                    //std::cout << "FAIL\n";
                }
            }
            if (match) {
                // Create patch entry where Bytes = "{VARIABLE}.{MODUUID}"
                //std::cout << "Entry for local var " << reloc[i].symbol << "\n";
                row.Value = "$ ";
                row.Value += reloc[i].symbol;
                row.Value += ".";
                row.Value += metadata.uuid;
                
                out.push_back(row);
                continue;
            }
                
            // If name is in import list:
            int import_entry = -1;
            for (int j = 0; j < imports.size(); j++) {
                for (int k = 0; k < imports[j].Variables.size(); k++) {
                    // Hackishly use strncmp to test symbol name against 
                    // first part of variable UUID.
                    //std::cout << "D: Testing " << reloc[i].symbol << " vs. " << imports[j].Variables[k].UUID << "...";
                    if (strncmp(reloc[i].symbol.c_str(), imports[j].Variables[k].UUID.c_str(), reloc[i].symbol.size()) == 0) {
                        //std::cout << "PASS\n";
                        import_entry = j;
                        break;
                    } else {
                        //std::cout << "FAIL\n";
                    }
                }
                if(import_entry >= 0){break;}
            }
            if (import_entry >= 0) {
                // Create patch entry where Bytes = "{VARIABLE}.{MODUUID}"
                //std::cout << "Entry for extern var " << reloc[i].symbol << "\n";
                row.Value = "$ ";
                row.Value += reloc[i].symbol;
                row.Value += ".";
                row.Value += imports[import_entry].UUID;
                
                out.push_back(row);
                continue;
            }
            
            // If name is a known patch:
            match = false;
            for (int j = 0; j < patches.size(); j++) {
                //std::cout << "E: Testing " << reloc[i].symbol << " vs. " << patches[j].ID << "...";
                // Hackishly use strncmp to test symbol name against 
                // first part of variable UUID.
                if (strncmp(patches[j].ID.c_str(), reloc[i].symbol.c_str(), reloc[i].symbol.size()) == 0) {
                    //std::cout << "PASS\n";
                    match = true;
                    break;
                } else {
                    //std::cout << "FAIL\n";
                }
            }
            if (match) {
                // Create patch entry where Bytes = "{VARIABLE}.{MODUUID}"
                //std::cout << "Entry for local var " << reloc[i].symbol << "\n";
                row.Value = "$ Start.";
                row.Value += reloc[i].symbol;
                row.Value += ".";
                row.Value += metadata.uuid;
                
                out.push_back(row);
                continue;
            }

            // Otherwise create patch entry where Bytes = "{VARIABLE}.MODLOADER@invisibleup"
            
            //std::cout << "Entry for global var " << reloc[i].symbol << "\n";
            row.Value = "$ Start.";
            row.Value += reloc[i].symbol;
            row.Value += ".MODLOADER@invisibleup";
            
            out.push_back(row);
        }
        
        return out;
    }
    
    void dump_zip(std::string outpath, struct archive *outzip) {
        
        std::vector<std::string> infiles;
        
        chdir(outpath.c_str());
        
        // Get filenames
        if (boost::filesystem::is_directory(".")) {

            std::ostringstream a;
            std::string b;

            // Iterators are terrible
            copy(
                boost::filesystem::recursive_directory_iterator("."),
                boost::filesystem::recursive_directory_iterator(), 
                std::ostream_iterator<boost::filesystem::directory_entry>(a, "\n")
            );
            
            b = a.str();
            
            boost::split(infiles, b, boost::is_any_of("\n"));
        }
        else {
            std::cout << outpath << " does not exist or is not a directory\n";
        }

        
        // Zip.
        for(int i = 0; i < infiles.size(); i++){
            // Remove quotes that exist for some reason
            infiles[i].erase(
                std::remove(
                    infiles[i].begin(), infiles[i].end(), '\"' 
                ), infiles[i].end()
            );
            
            ///std::cerr << infiles[i] << "...\n";
            
            if(infiles[i] == ""){continue;}
            
            struct archive *ard = archive_read_disk_new();
            archive_read_disk_set_standard_lookup(ard);
            struct archive_entry * ent = archive_entry_new(); 
            
            int file = open(infiles[i].c_str(), O_RDONLY);
            if(file == -1){
                std::cerr << "Could not open file " << infiles[i] << "!\n";
                break;
            }
            
            // The "+2" gets rid of the leading "./"
            archive_entry_set_pathname(ent, infiles[i].c_str()+2);
            ///std::cerr << "> " << archive_entry_pathname(ent) << "\n";
            archive_read_disk_entry_from_file(ard, ent, file, NULL);
            archive_write_header(outzip, ent);
            
            char buff[1024];
            int len = read(file, (void *)buff, sizeof(buff));
            while (len > 0) {
                archive_write_data(outzip, buff, len);
                len = read(file, buff, sizeof(buff));
            }
            
            archive_write_finish_entry(outzip);
            archive_read_free(ard);
            archive_entry_free(ent);
            close(file);
        }
        chdir("../../");
    }
    
public:
    
    Compiler _compiler;
    
    Mod(Compiler compiler) {
        // Load manually added stuff
        _compiler = compiler;
        std::string path = compiler.Path;
        metadata = Metadata(path + "/metadata.json");
        variables = load_variables(path + "/variables.json");
        imports = load_imports(path + "/imports.json");
        patches = load_patches(path + "/patches.json");
        functions = load_functions(path + "/functions.json", compiler);
        
        // Convert functions to patches, compiling in process
        for(int i = 0; i < functions.size(); i++){
            patches = Function2Patches(functions[i], patches);
        }
        
        std::vector<Patch> patches2 = load_patches(path + "/patches-post.json");
        if(!patches2.empty()){
            patches.insert(patches.end(), patches2.begin(), patches2.end() );
        }
    }
    
    void dump() {
        json_t *out = metadata.json;
        json_t *vars = json_array();
        
        // Imports/Dependencies
        if(!imports.empty()){
            json_t *deps = json_array();
            
            for (int i = 0; i < imports.size(); i++) {
                json_t *row = json_object();
                json_object_set_new(row, "Name", json_string(imports[i].Name.c_str()));
                json_object_set_new(row, "Ver", json_string(imports[i].Ver.c_str()));
                json_object_set_new(row, "Auth", json_string(imports[i].Auth.c_str()));
                json_object_set_new(row, "UUID", json_string(imports[i].UUID.c_str()));
                json_array_append_new(deps, row);
                                
                // Add variable updates defined in imports
                for (int j = 0; j < imports[i].Variables.size(); j++) {
                    json_t *row = json_object();
                    json_object_set_new(row, "UUID", json_string(imports[i].Variables[j].UUID.c_str()));
                    json_object_set_new(row, "Type", json_string(imports[i].Variables[j].Type.c_str()));
                    json_object_set_new(row, "Update", json_string(imports[i].Variables[j].Value.c_str()));
                    json_array_append_new(vars, row);
                }

            }
            json_object_set_new(out, "dependencies", deps);
        }
        
        // Variables
        if(!variables.empty()){
            for (int i = 0; i < variables.size(); i++) {
                json_t *row = json_object();
                
                json_object_set_new(row, "UUID", json_string(variables[i].UUID.c_str()));
                json_object_set_new(row, "Type", json_string(variables[i].Type.c_str()));
                if (variables[i].Mode == Variable::NEW) {
                    json_object_set_new(row, "Default", json_string(variables[i].Value.c_str()));
                } else {
                    json_object_set_new(row, "Update", json_string(variables[i].Value.c_str()));
                }
                json_object_set_new(row, "Desc", json_string(variables[i].Desc.c_str()));
                json_object_set_new(row, "PublicType", json_string(variables[i].PublicType.c_str()));
                
                json_array_append_new(vars, row);
            }
            
            
        }
        json_object_set_new(out, "variables", vars);
        
        // Patches
        if(!patches.empty()){
            json_t *ptchs = json_array(); // Terrible naming scheme in hindsight
            for (int i = 0; i < patches.size(); i++) {
                json_t *row = json_object();
                
                // Yikes
                json_object_set_new(row, "ID", json_string(patches[i].ID.c_str()));
                json_object_set_new(row, "Mode", json_string(patches[i].Mode.c_str()));
                json_object_set_new(row, "File", json_string(patches[i].File.c_str()));
                json_object_set_new(row, "FileType", json_string(patches[i].FileType.c_str()));
                json_object_set_new(row, "Start", json_string(patches[i].Start.c_str()));
                json_object_set_new(row, "End", json_string(patches[i].End.c_str()));
                json_object_set_new(row, "Len", json_string(patches[i].Len.c_str()));
                json_object_set_new(row, "SrcFile", json_string(patches[i].SrcFile.c_str()));
                json_object_set_new(row, "SrcFileType", json_string(patches[i].SrcFileType.c_str()));
                json_object_set_new(row, "SrcFileLoc", json_string(patches[i].SrcFileLoc.c_str()));
                json_object_set_new(row, "SrcStart", json_string(patches[i].SrcStart.c_str()));
                json_object_set_new(row, "SrcEnd", json_string(patches[i].SrcEnd.c_str()));
                json_object_set_new(row, "AddType", json_string(patches[i].AddType.c_str()));
                json_object_set_new(row, "Value", json_string(patches[i].Value.c_str()));
                json_object_set_new(row, "Condition", json_string(patches[i].Condition.c_str()));
                
                json_array_append_new(ptchs, row);
            }
            
            json_object_set_new(out, "patches", ptchs);
        }
        
        // Save JSON
        std::string outpath = _compiler.Path;
        outpath += "/out/";
        std::string json_path = outpath;
        json_path += "info.json";
        json_dump_file(out, json_path.c_str(), JSON_INDENT(4));
        
        // Copy data/, if it exists, to out/
        std::string datasrc = _compiler.Path;
        datasrc += "/data/";
        std::string datadest = outpath;
        datadest += "data/";
        std::cout << "Copying " << datasrc << " to " << datadest << "...\n";
        CopyDir(datasrc, datadest);
        
        // Create archive
        std::string zippath;// = outpath;
        zippath = metadata.uuid;
        zippath += ".zip";
        struct archive *outzip = archive_write_new();
        archive_write_set_format_zip(outzip);
        archive_write_open_filename(outzip, zippath.c_str());

        dump_zip(outpath, outzip);
        archive_write_close(outzip);
        archive_write_free(outzip);
    }
};

int main(int argc, char **argv)
{
    // Get input directory
    if(argc != 2){
        std::cerr << "SrMdLdr Mod Generator v1.0\n";
        std::cerr << "Usage: " << argv[0] << " {Directory w/ mod}" << std::endl;
        return 1;
    }
    std::string inputdir = std::string(argv[1]);
    
    bfd_init();
    
    // Get compiler info
    Compiler compiler = Compiler(inputdir);
    
    // Slurp metadata, variables, imports, etc.
    Mod mod = Mod(compiler);
    
    // Write output
    mod.dump();
    
    return 0;
}
