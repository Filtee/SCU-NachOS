/* add.c
 *	Simple program to test whether the systemcall interface works.
 *	
 *	Just do a add syscall that adds two values and returns the result.
 *
 */

#include "syscall.h"

int
main() {
    int result, another;

    result = Add(1, 2);
    another = 3;
    result = Add(result, another);

    Halt();
    /* not reached */
}
