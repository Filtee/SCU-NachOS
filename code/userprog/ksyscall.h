/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


void SysHalt() {
    kernel->interrupt->Halt();
}


int SysAdd(int op1, int op2) {
    cout << "************************系统调用结果****************" << endl;
    cout << op1 << " + " << op2 << " = " << op1 + op2 << endl;
    return op1 + op2;

}

////////
int SysWrite(int buf, int size, int id) {
    char buffer[128];
    int count = 0;
    int ch;
    do {
        kernel->machine->ReadMem(buf, 1, &ch);
        buf++;
        buffer[count] = (char) ch;
    } while (ch != '\0' && count++ <= 127);
    return write(id, buffer, (size_t) size);
}

int SysRead(char *buffer, int size, OpenFileId id) {
    return read(id, buffer, size);
}

int SysJoin(SpaceId id) {
    return waitpid(id, (int *) 0, 0);
}

SpaceId SysExec(char *exec_name) {
    char *argv[] = {"bash", "-c", exec_name, NULL};
    pid_t pid;
    if (pid = fork() == 0) {
        execvp("bash", argv);
        exit(0);
    } else if (pid == 1) {
        return (SpaceId) pid;
    } else
        return -1;
}
////////


#endif /* ! __USERPROG_KSYSCALL_H__ */

