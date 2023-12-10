// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize        1024    // increase this as necessary!

class AddrSpace {
public:
    // Create an address space.
    AddrSpace();

    // De-allocate an address space.
    ~AddrSpace();

    // Load a program into addr space from a file,
    // return false if not found.
    bool Load(char *fileName);

    // Run a program assumes the program has already been loaded.
    void Execute();

    // Save/restore address space-specific, info on a context switch.
    void SaveState();

    void RestoreState();

    // Store the meta information of the current program,
    // such as the size and virtual address of the code and data.
    int* FileAddr;

    // Translate virtual address _vaddr_
    // to physical address _paddr_. _mode_
    // is 0 for Read, 1 for Write.
    ExceptionType Translate(unsigned int vaddr, unsigned int *paddr, int mode);

private:
    // Assume linear page table translation for now!
    TranslationEntry *pageTable;

    // Number of pages in the virtual address space.
    unsigned int numPages;

    // Initialize user-level CPU registers, before jumping to user code.
    void InitRegisters();

    // Temporary storage of registers.
    int* s_reg;
};

#endif // ADDRSPACE_H
