// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	asm volatile("int $3");


	cprintf("1:this is after int 3\n");

	cprintf("2:this is after int 3\n");
	
}

