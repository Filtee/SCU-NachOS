// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	is in machine.h.
//----------------------------------------------------------------------

void
incrementPC() {
    /* set previous programm counter (debugging only)*/
    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(NextPCReg));

    /* set next programm counter for brach execution */
    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(NextPCReg) + 4);
}

void
ExceptionHandler(ExceptionType which) {
    int type = kernel->machine->ReadRegister(2);

    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

    switch (which) {
        case SyscallException:
            switch (type) {
                case SC_Halt:
                    DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

                    SysHalt();

                    ASSERTNOTREACHED();
                    break;

                case SC_Add:
                    DEBUG(dbgSys,
                          "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5)
                                 << "\n");

                    /* Process SysAdd Systemcall*/
                    int result;
                    result = SysAdd(/* int op1 */(int) kernel->machine->ReadRegister(4),
                            /* int op2 */(int) kernel->machine->ReadRegister(5));

                    DEBUG(dbgSys, "Add returning with " << result << "\n");
                    /* Prepare Result */
                    kernel->machine->WriteRegister(2, (int) result);

                    /* Modify return point */
                    {
                        /* set previous programm counter (debugging only)*/
                        kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

                        /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                        kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

                        /* set next programm counter for brach execution */
                        kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    }

                    return;
                    ASSERTNOTREACHED();
                    break;

                case SC_Write:
                    /* Debug notation. */
                    DEBUG(dbgSys, "Write from buffer to consoleOutput" << kernel->machine->ReadRegister(4)
                                                                       << kernel->machine->ReadRegister(5)
                                                                       << kernel->machine->ReadRegister(6) << "\n");

                    /* Read from registers. */
                    int addr;
                    addr = (int) kernel->machine->ReadRegister(4);
                    int size;
                    size = (int) kernel->machine->ReadRegister(5);
                    OpenFileId output;
                    output = (OpenFileId) kernel->machine->ReadRegister(6);

                    /* System write and load the writing result onto register 2. */
                    kernel->machine->WriteRegister(2, SysWrite(addr, size, output));

                    /* Increment PC register by 1, and return the system writing result. */
                    incrementPC();
                    return;
                    ASSERTNOTREACHED();
                    break;

                case SC_Read:
                    /* Debug notation. */
                    DEBUG(dbgSys, "Read file to buffer" << kernel->machine->ReadRegister(4) << ","
                                                        << kernel->machine->ReadRegister(5) << "words" << "\n");

                    /* Read from registers. */
                    addr = (int) kernel->machine->ReadRegister(4);
                    size = (int) kernel->machine->ReadRegister(5);
                    OpenFileId input;
                    input = (OpenFileId) kernel->machine->ReadRegister(6);

                    /* Call System read and load the result onto register 2. */
                    result = SysRead(addr, size, input);
                    kernel->machine->WriteRegister(2, (int) result);

                    /* Increment PC register by 1, and return the system writing result. */
                    incrementPC();
                    return;
                    ASSERTNOTREACHED();
                    break;

                case SC_Exec:
                    /* Debug notation. */
                    DEBUG(dbgSys, "execute a command" << kernel->machine->ReadRegister(4) << "\n");

                    /* Read from register 4 => the address of the input buffer (input command). */
                    addr = kernel->machine->ReadRegister(4);

                    /* Call System Execute and load the child process id onto register 2. */
                    pid_t child;
                    child = SysExec(addr);
                    kernel->machine->WriteRegister(2, (int) child);

                    /* Increment PC register by 1, and return the system writing result. */
                    incrementPC();
                    return;
                    ASSERTNOTREACHED();
                    break;

                case SC_Join:
                    /* Debug notation. */
                    DEBUG(dbgSys, "join" << kernel->machine->ReadRegister(4) << "\n");

                    /* Read from register 4 => the input child process id. */
                    child = (pid_t) kernel->machine->ReadRegister(4);

                    /* Call System Join to wait the process to be executed. */
                    result = SysJoin(child);
                    kernel->machine->WriteRegister(2, (int) result);

                    /* Increment PC register by 1, and return the system writing result. */
                    incrementPC();
                    return;
                    ASSERTNOTREACHED();
                    break;

                default:
                    cerr << "Unexpected system call " << type << "\n";
                    break;
            }
            break;

        case PageFaultException:
            int virAddr, vpn, offset, phy, vir;
            //磁盘文件
            OpenFile *out_file, *in_file;
            //待读取的虚拟entry和待替换的entry
            virAddr = kernel->machine->ReadRegister(BadVAddrReg);
            //虚拟页号
            vpn = virAddr / PageSize;
            //偏移量
            offset = virAddr % PageSize;
            cout << "virtual address " << virAddr << " with page " << vpn << " offset " << offset
                 << " triggering a page fault!" << endl << endl;

            //实际的替换物理页号
            phy = kernel->machine->findFreeFrame(vpn, kernel->machine->pageTable);

            //没有空闲的页了
            if (phy == -1) {
                //待替换的全局页表中的物理页号
                phy = kernel->machine->findFreeByLU();
                //待替换的全局页表中的原引用的虚拟页号
                vir = kernel->machine->GlobalPageTable[phy].VirNum;

                cout << "no free frame and choose virtual page " << vir << " memory frame " << phy
                     << " as replacement by LU!" << endl;

                //读取程序的名称便于将程序从磁盘读入到内存中
                char *fileName = kernel->machine->GlobalPageTable[phy].RefPageTable[vir].DiskFile;

                out_file = kernel->fileSystem->Open(fileName);
                cout << "previous frame will be write back to " << fileName << endl;

                ASSERT(out_file != NULL)
                //如果是该Frame被写过，才会写回disk
                if (kernel->machine->GlobalPageTable[phy].RefPageTable[vir].dirty) {
                    out_file->WriteAt(&(kernel->machine->mainMemory[phy * PageSize]), PageSize,
                                      kernel->machine->GlobalPageTable[phy].RefPageTable[vir].virtualPage * PageSize);
                }
                cout << "successfully write frame " << phy << " back to disk!" << endl;
                delete out_file;
            }
            //else cout << "free memory frame " << phy << " is used to tackle page fault!" << endl;
            in_file = kernel->fileSystem->Open(kernel->machine->pageTable[vpn].DiskFile);
            cout << "Opening " << kernel->machine->pageTable[vpn].DiskFile << " !" << endl;
            ASSERT(in_file != NULL)

            //读取程序的元信息
            int cVir, cSize, cIn, dVir, dSize, dIn, roVir, roSize, roIn;
            cVir = kernel->machine->FileAddr[0];
            cSize = kernel->machine->FileAddr[1];
            cIn = kernel->machine->FileAddr[2];
            dVir = kernel->machine->FileAddr[3];
            dSize = kernel->machine->FileAddr[4];
            dIn = kernel->machine->FileAddr[5];
            roVir = kernel->machine->FileAddr[6];
            roSize = kernel->machine->FileAddr[7];
            roIn = kernel->machine->FileAddr[8];

            //逐字节将虚拟地址对应的页的内容从磁盘写入内存中找到的物理页中
            for (int i = 0; i < PageSize; i++) {
                int vAddr = vpn * PageSize + i;
                int pAddr = phy * PageSize + i;
                //如果该数据位于代码段
                if (vAddr >= cVir && vAddr < (cVir + cSize)) {
                    //cout << "********code fault!********" << endl;
                    in_file->ReadAt(&(kernel->machine->mainMemory[pAddr]), 1, cIn + vAddr - cVir); // cIn+vAddr-cVir
                }//如果在数据段
                else if (vAddr >= dVir && vAddr < (dVir + dSize)) {
                    //cout << "********data fault!********" << endl;
                    in_file->ReadAt(&(kernel->machine->mainMemory[pAddr]), 1, dIn + vAddr - dVir); //-dVir
                }
                    //如果在只读数据段
                else if (vAddr >= roVir && vAddr < (roVir + roSize)) {
                    in_file->ReadAt(&(kernel->machine->mainMemory[pAddr]), 1, roIn + vAddr - roVir);
                } else {
                    //执行到这里，说明目前发生的缺页的原本目的并非读入程序的代码和数据，而是开辟新的空间用于存储
                    /*
                    cout << "**********不能识别的缺页虚拟地址************" << endl;
                    cout << "虚拟基址: " << virAddr << endl;
                    cout << "错误虚拟地址: " << vAddr << endl;
                    if (cSize > 0) cout << "代码段范围: " << cVir << " ," << cVir+cSize << endl;
                    if (dSize > 0) cout << "数据段范围: " << dVir << " ," << dVir+dSize << endl;
                    if (roSize > 0) cout << "只读段范围: " << roVir << " ," << roVir+roSize << endl;
                    cout << "******************************************" << endl;*/
                    continue;
                    //ASSERT(FALSE);
                }
            }

            //in_file->ReadAt(&(kernel->machine->mainMemory[phy*PageSize]), PageSize, kernel->machine->pageTable[vpn].virtualPage*PageSize);
            cout << "successfully writing frame " << vpn << " to memory from disk!" << endl;

            //更新全局页表:将旧的的物理地址对应的全局页表项删除，同时更新缺页的虚拟地址信息
            cout << "global update: previous virtual page " << vir << ", physical page " << phy
                 << " referencing is invalid!" << endl;
            //因为该原页的物理地址被占，因此原引用地址空间的页表对应的虚拟页失效
            if (kernel->machine->GlobalPageTable[phy].RefPageTable != NULL)
                kernel->machine->GlobalPageTable[phy].RefPageTable[vir].valid = FALSE;
            //更新的引用该物理页的虚拟页号
            kernel->machine->GlobalPageTable[phy].VirNum = kernel->machine->pageTable[vpn].virtualPage;
            //更新的引用该物理页的地址空间的页表
            kernel->machine->GlobalPageTable[phy].RefPageTable = kernel->machine->pageTable;
            kernel->machine->GlobalPageTable[phy].useStamp = 0;
            cout << "new global entry : " << phy << " , " << kernel->machine->pageTable[vpn].virtualPage << endl;

            //更新地址空间的程序页表
            kernel->machine->pageTable[vpn].physicalPage = phy;
            kernel->machine->pageTable[vpn].valid = TRUE;
            kernel->machine->pageTable[vpn].use = FALSE;
            kernel->machine->pageTable[vpn].dirty = FALSE;
            kernel->machine->pageTable[vpn].readOnly = FALSE;
            delete in_file;
            cout << "page fault exception ends!new GlobalPageTable:" << endl;

            //打印全局页表
            kernel->machine->printGlbPt();
            return;
            ASSERTNOTREACHED();
            break;

        default:
            cerr << "Unexpected user mode exception" << (int) which << "\n";
            break;
    }
    ASSERTNOTREACHED();
}
// Machine::ReadMem
//	"addr" -- the virtual address to read from
//	"size" -- the number of bytes to read (1, 2, or 4)
//	"value" -- the place to write the result