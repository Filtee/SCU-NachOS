/* add.c
 *	Simple program to test whether the systemcall interface works.
 *	
 *	Just do a add syscall that adds two values and returns the result.
 *
 */

#include "syscall.h"

//修改add测试程序
int
main() {
    int result, another;

    result = Add(1, 2);
    another = 3;
    result = Add(result, another);

    Halt();
    /* not reached */
}
