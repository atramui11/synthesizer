#include <stdio.h>
#include <x86.h>
#include <syscall.h>

int main(int argc, char **argv)
{
    printf("piano main started.\n\n\n\n\n\n");
	
    //play sound thru system call
    sys_playsound();	

    return 0;
}
