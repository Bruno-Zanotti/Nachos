/// Routines to manage a directory of file names.
///
/// The directory is a table of fixed length entries; each entry represents a
/// single file, and contains the file name, and the location of the file
/// header on disk.  The fixed size of each directory entry means that we
/// have the restriction of a fixed maximum size for file names.
///
/// The constructor initializes an empty directory of a certain size; we use
/// ReadFrom/WriteBack to fetch the contents of the directory from disk, and
/// to write back any modifications back to disk.
///
/// Also, this implementation has the restriction that the size of the
/// directory cannot expand.  In other words, once all the entries in the
/// directory are used, no more files can be created.  Fixing this is one of
/// the parts to the assignment.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "directory.hh"
#include "directory_entry.hh"
#include "file_header.hh"
#include "lib/utility.hh"
#include "file_system.hh"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/// Initialize a directory; initially, the directory is completely empty.  If
/// the disk is being formatted, an empty directory is all we need, but
/// otherwise, we need to call FetchFrom in order to initialize it from disk.
///
/// * `size` is the number of entries in the directory.
Directory::Directory()
{
    raw.table = nullptr;
    raw.tableSize = 0;
}

Directory::Directory(unsigned size, const char *_name)
{
    ASSERT(size > 0);
    raw.table = new DirectoryEntry [size];
    raw.tableSize = size;
    for (unsigned i = 0; i < raw.tableSize; i++)
        raw.table[i].inUse = false;
    name = _name;
}

/// De-allocate directory data structure.
Directory::~Directory()
{
    delete [] raw.table;
}

/// Read the contents of the directory from disk.
///
/// * `file` is file containing the directory contents.
void
Directory::FetchFrom(OpenFile *file)
{
    ASSERT(file != nullptr);
    file->ReadAt((char *) &raw.tableSize, 1, 0);
    if(raw.tableSize > 0){
        raw.table = (DirectoryEntry*) realloc(raw.table, raw.tableSize * sizeof(DirectoryEntry));
        file->ReadAt((char *) raw.table, raw.tableSize * sizeof (DirectoryEntry), 1);
    }
}

/// Write any modifications to the directory back to disk.
///
/// * `file` is a file to contain the new directory contents.
void
Directory::WriteBack(OpenFile *file)
{
    ASSERT(file != nullptr);
    file->WriteAt((char *) &raw.tableSize, 1, 0);
    if (raw.tableSize > 0)
        file->WriteAt((char *) raw.table, raw.tableSize * sizeof (DirectoryEntry), 1);
}

/// Look up file name in directory, and return its location in the table of
/// directory entries.  Return -1 if the name is not in the directory.
///
/// * `name` is the file name to look up.
int
Directory::FindIndex(const char *_name)
{
    ASSERT(_name != nullptr);
    DEBUG('f', "Finding index of file: %s\n", _name);
    for (unsigned i = 0; i < raw.tableSize; i++)
        if (raw.table[i].inUse && !strncmp(raw.table[i].name, _name, FILE_NAME_MAX_LEN)){
            DEBUG('f', "Found index of file: %s, index: %u\n", _name, i);
            return i;
        }
    DEBUG('f', "Index of file: %s not found\n", _name);
    return -1;  // name not in directory
}

/// Look up file name in directory, and return the disk sector number where
/// the file's header is stored.  Return -1 if the name is not in the
/// directory.
///
/// * `name` is the file name to look up.
int
Directory::Find(const char *_name)
{
    ASSERT(_name != nullptr);

    int i = FindIndex(_name);
    if (i != -1)
        return raw.table[i].sector;
    return -1;
}

/// Add a file into the directory.  Return true if successful; return false
/// if the file name is already in the directory, or if the directory is
/// completely full, and has no more space for additional file names.
///
/// * `name` is the name of the file being added.
/// * `newSector` is the disk sector containing the added file's header.
bool
Directory::Add(const char *_name, int newSector, bool isDirectory)
{
    ASSERT(_name != nullptr);
    DEBUG('f', "Adding file: %s in directory in sector %d and isDirectory:%d\n", _name, newSector, isDirectory);

    if (FindIndex(_name) != -1)
        return false;

    for (unsigned i = 0; i < raw.tableSize; i++)
        if (!raw.table[i].inUse) {
            raw.table[i].inUse = true;
            raw.table[i].isDirectory = isDirectory;
            strncpy(raw.table[i].name, _name, FILE_NAME_MAX_LEN);
            raw.table[i].sector = newSector;
            return true;
        }
    if (Expand()){
        unsigned lastIndex = raw.tableSize - 1;
        raw.table[lastIndex].inUse = true;
        raw.table[lastIndex].isDirectory = isDirectory;
        strncpy(raw.table[lastIndex].name, _name, FILE_NAME_MAX_LEN);
        raw.table[lastIndex].sector = newSector;
        return true;
    }
    return false;  // no space.  Fix when we have extensible files.
    
}

/// Remove a file name from the directory.   Return true if successful;
/// return false if the file is not in the directory.
///
/// * `name` is the file name to be removed.
bool
Directory::Remove(const char *_name)
{
    ASSERT(_name != nullptr);

    int i = FindIndex(_name);
    if (i == -1)
        return false;  // name not in directory
    raw.table[i].inUse = false;
    return true;
}

/// Make the directory bigger.
/// Return true if successful;
bool 
Directory::Expand(){
    DirectoryEntry *newTable = new DirectoryEntry [raw.tableSize+1];
    for (unsigned i = 0; i < raw.tableSize; i++)
        newTable[i] = raw.table[i];
    
    newTable[raw.tableSize].inUse = false;

    raw.tableSize ++; 
    raw.table = newTable;
    return true;
}

/// List all the file names in the directory.
void
Directory::List() const
{
    for (unsigned i = 0; i < raw.tableSize; i++)
        if (raw.table[i].inUse)
            printf("%s\n", raw.table[i].name);
}

/// List all the file names in the directory, their `FileHeader` locations,
/// and the contents of each file.  For debugging.
void
Directory::Print() const
{
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (unsigned i = 0; i < raw.tableSize; i++)
        if (raw.table[i].inUse) {
            printf("\nDirectory entry:\n"
                   "    name: %s\n"
                   "    sector: %u\n",
                   raw.table[i].name, raw.table[i].sector);
            hdr->FetchFrom(raw.table[i].sector);
            hdr->Print(nullptr);
        }
    printf("\n");
    delete hdr;
}

const RawDirectory *
Directory::GetRaw() const
{
    return &raw;
}

const char *
Directory::GetName()
{
    return name;
}