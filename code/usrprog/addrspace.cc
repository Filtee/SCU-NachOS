// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr = 
           WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = 
           WordToHost(noffH->readonlyData.inFileAddr);
#endif 
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size <<  
                   " readonly = " << noffH->readonlyData.size <<
                   " init = " << noffH->initData.size <<
                   " uninit = " << noffH->uninitData.size << "\n");
#endif
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{   
    cout << "creating a new addrSpace!" << endl;
    //申请暂存的寄存器buffer的空间
    s_reg = new int[NumTotalRegs];
    // zero out the entire address space
    //bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   for (int i=0; i < NumPhysPages; i++){
	if (pageTable[i].physicalPage != -1){
		//只清除自己的地址空间中占用的物理内存页
		bzero(&(kernel->machine->mainMemory[pageTable[i].physicalPage*PageSize]), PageSize);
		//同时将全局页表的引用改为null，使得其可以作为freeframe被找到
		kernel->machine->GlobalPageTable[pageTable[i].physicalPage].RefPageTable = NULL;
		kernel->machine->GlobalPageTable[pageTable[i].physicalPage].VirNum = -1;
	}
   }
   delete pageTable;
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool 
AddrSpace::Load(char *fileName) 
{
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
	cerr << "Unable to open file " << fileName << "\n";
	return FALSE;
    }

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

#ifdef RDATA
// how big is address space?
    size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size +
           noffH.uninitData.size + UserStackSize;	
                                                // we need to increase the size
						// to leave room for the stack
#else
// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
#endif
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
    cout << "loading program " << fileName << endl;
    pageTable = new TranslationEntry[NumPhysPages];
    //本来应该是 i < numPages，但是由于程序所用的page过少，如果按需分配将会导致无法测试缺页
    //因此这里改为 i < NumPhysPages， 即有多少个分配多少个，可以导致足够的缺页数量进行测试
    for (int i = 0; i < NumPhysPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virt page # = phys page #
	pageTable[i].physicalPage = kernel->machine->findFreeFrame(i, pageTable);     //修改为寻找空闲的Frame
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  
	//存储本程序加载的程序名称，用于在缺页中写回页面
	pageTable[i].DiskFile = fileName;
    }

    DEBUG(dbgAddr, "Initializing address space: " << numPages << ", " << size);
    cout << "Initializing address space: " << numPages << " , " << size << endl;

    //存储本程序的元信息
    FileAddr = new int[9];
    FileAddr[0] = noffH.code.virtualAddr;
    FileAddr[1] = noffH.code.size;
    FileAddr[2] = noffH.code.inFileAddr;
    FileAddr[3] = noffH.initData.virtualAddr;
    FileAddr[4] = noffH.initData.size;
    FileAddr[5] = noffH.initData.inFileAddr;
    FileAddr[6] = noffH.readonlyData.virtualAddr;
    FileAddr[7] = noffH.readonlyData.size;
    FileAddr[8] = noffH.readonlyData.inFileAddr;

    cout << "**************** Page Table ****************" << endl;
    //kernel->machine->printPt();
    for (int i=0; i < PageSize; i++){
	cout << "virtual page "<< pageTable[i].virtualPage << " physical page: " << pageTable[i].physicalPage << endl;
    }

// then, copy in the code and data segments into memory
// Note: this code assumes that virtual address = physical address
    int add, vaddr;
    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");
	DEBUG(dbgAddr, noffH.code.virtualAddr << ", " << noffH.code.size);
	//逐字节读入代码，但是需要把虚拟地址通过页表转化为物理地址，以下同
	//转换公式：virtualAddr/PageSize得到虚拟页号，利用该虚拟页号查页表得到物理页号
	//再利用物理页号*PageSize得到对应物理页起始地址，再加上virtualAddr%PageSize得到页内偏移得到物理地址
	for (int i=0; i < noffH.code.size; i++){
		vaddr = noffH.code.virtualAddr+i;
		add = pageTable[vaddr/PageSize].physicalPage*PageSize + vaddr%PageSize;
		//只有分配到物理页的数据才会读入
		if (pageTable[vaddr/PageSize].physicalPage >= 0){
			executable->ReadAt(&(kernel->machine->mainMemory[add]), 1, noffH.code.inFileAddr+i);		
		}
	}
	/*
	if (pageTable[noffH.code.virtualAddr/PageSize].physicalPage >= 0){
		add = pageTable[noffH.code.virtualAddr/PageSize].physicalPage*PageSize + noffH.code.virtualAddr%PageSize;
		cout << "code:           vir add: " << noffH.code.virtualAddr << " phy add: " << add << endl;
		executable->ReadAt(
			&(kernel->machine->mainMemory[add]), 
				noffH.code.size, noffH.code.inFileAddr);
	}
	executable->ReadAt(&(kernel->machine->mainMemory[noffH.code.virtualAddr]),
				noffH.code.size, noffH.code.inFileAddr);*/
    }
    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");
	DEBUG(dbgAddr, noffH.initData.virtualAddr << ", " << noffH.initData.size);
	/*
	if (pageTable[noffH.initData.virtualAddr/PageSize].physicalPage >= 0){
		add = pageTable[noffH.initData.virtualAddr/PageSize].physicalPage*PageSize + noffH.initData.virtualAddr%PageSize;
		cout << "data:           vir add: " << noffH.initData.virtualAddr << " phy add: " << add << endl;
		executable->ReadAt(
			&(kernel->machine->mainMemory[add]),
				noffH.initData.size, noffH.initData.inFileAddr);
	}

	executable->ReadAt(&(kernel->machine->mainMemory[noffH.initData.virtualAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);*/
    }

#ifdef RDATA
    if (noffH.readonlyData.size > 0) {
        DEBUG(dbgAddr, "Initializing read only data segment.");
	DEBUG(dbgAddr, noffH.readonlyData.virtualAddr << ", " << noffH.readonlyData.size);
	for (int i=0; i < noffH.readonlyData.size; i++){
		vaddr = noffH.readonlyData.virtualAddr+i;
		add = pageTable[vaddr/PageSize].physicalPage*PageSize + vaddr%PageSize;
		if (pageTable[vaddr/PageSize].physicalPage >= 0){
			executable->ReadAt(&(kernel->machine->mainMemory[add]), 1, noffH.readonlyData.inFileAddr+i);		
		}
	}
	/*
	if (pageTable[noffH.readonlyData.virtualAddr/PageSize].physicalPage >= 0){
		add = pageTable[noffH.readonlyData.virtualAddr/PageSize].physicalPage*PageSize + noffH.readonlyData.virtualAddr%PageSize;
		executable->ReadAt(
			&(kernel->machine->mainMemory[add]),
				noffH.readonlyData.size, noffH.readonlyData.inFileAddr);
	}
	executable->ReadAt(&(kernel->machine->mainMemory[noffH.readonlyData.virtualAddr]),
				noffH.readonlyData.size, noffH.readonlyData.inFileAddr);*/
    }
#endif

    delete executable;			// close file
    cout << "Successfully loading " << fileName << endl;
    return TRUE;			// success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//
//      The program is assumed to have already been loaded into
//      the address space
//
//----------------------------------------------------------------------

void 
AddrSpace::Execute() 
{
    cout << "**************Execute!**************" << endl;
    kernel->currentThread->space = this;

    this->InitRegisters();		// set the initial register values

    //不能直接调用restore方法，因为会覆盖掉寄存器的初始化
    //应该将pageTable等信息直接赋值给machine中的pageTable等变量
    //this->RestoreState();		// load page table register
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
    kernel->machine->FileAddr = FileAddr;

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start", which
    //  is assumed to be virtual address zero
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    // Since instructions occupy four bytes each, the next instruction
    // after start will be at virtual address four.
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{	
	cout << "saving state!" << endl;
	//暂存寄存器内容
	for (int i=0; i < NumTotalRegs; i++){
		s_reg[i] = kernel->machine->ReadRegister(i);
	}
	cout << "Successfully saving state!" << endl;
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    cout << "restoring state!" << endl;
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
    //也要写入加载程序的元信息
    kernel->machine->FileAddr = FileAddr;
    //将暂存内容写回寄存器
    for (int i=0; i < NumTotalRegs; i++){
	kernel->machine->WriteRegister(i, s_reg[i]);
    }
    cout << "successfully restoring state!" << endl;
}


//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//  The flag _isReadWrite_ is false (0) for read-only access; true (1)
//  for read-write access.
//  Return any exceptions caused by the address translation.
//----------------------------------------------------------------------
ExceptionType
AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr, int isReadWrite)
{
    TranslationEntry *pte;
    int               pfn;
    unsigned int      vpn    = vaddr / PageSize;
    unsigned int      offset = vaddr % PageSize;

    if(vpn >= numPages) {
        return AddressErrorException;
    }

    pte = &pageTable[vpn];

    if(isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;
    
    //如果物理内存对应的页号为-1，代表缺页
    if (pfn == -1 || !pte->valid)
	return PageFaultException;

    // if the pageFrame is too big, there is something really wrong!
    // An invalid translation was loaded into the page table or TLB.
    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    pte->use = TRUE;          // set the use, dirty bits

    if(isReadWrite)
        pte->dirty = TRUE;

    *paddr = pfn*PageSize + offset;

    ASSERT((*paddr < MemorySize));

    //cerr << " -- AddrSpace::Translate(): vaddr: " << vaddr <<
    //  ", paddr: " << *paddr << "\n";

    return NoException;
}









