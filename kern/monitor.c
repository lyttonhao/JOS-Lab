// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "backtrace", mon_backtrace},
	{ "showmappings", "Display physical page mappings and permission bits", mon_showmappings},
	{ "change_perm", "Change permission of pages", mon_change_perm},
	{ "dump", "Dump the contents of a range", mon_dump},
	{ "continue", "continue execution frome the current location", mon_continue},
	{ "singlestep", "single-step one instruction", mon_singlestep}

};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int mon_singlestep(int argc, char **argv, struct Trapframe *tf)
{
	if (tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG)
	{
		cprintf("The trapno is not Break Point or Debug\n");
		return -1;
	}
	cprintf("single step\n");


	struct Eipdebuginfo info;
	uintptr_t addr;

	addr = (uintptr_t) tf->tf_eip;
	debuginfo_eip(addr, &info);
	cprintf("Now instructoin is:%s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
		info.eip_fn_namelen, info.eip_fn_name,
		(addr-info.eip_fn_addr));

	extern struct Env *curenv;
	tf->tf_eflags |= FL_TF;
	env_run(curenv);

	return 0;
}


int mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	if (tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG)
	{
		cprintf("The trapno is not Break Point or Debug\n");
		return -1;
	}
	cprintf("continue to execute\n");

	tf->tf_eflags &= ~FL_TF;
	
	extern struct Env *curenv;
	env_run(curenv);

	return 0;
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{

	if (argc != 3) cprintf("showmappings error");
	else showmappings(argc, argv, tf);

	return 0;
}

int mon_change_perm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) cprintf("change_perm error");
	else change_perm(argc, argv, tf);

	return 0;
}

int mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 4) cprintf("dump error");
	else dump(argc, argv, tf);

	return 0;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint32_t *Ebp;
    uint32_t ebp, eip, arg[5], i;
	struct Eipdebuginfo info;
	uintptr_t addr;

	cprintf("Stack backtrace:\n");
	Ebp = (uint32_t *) read_ebp();
	while (Ebp != 0)
	{
		ebp =(int) Ebp;
		eip = *(Ebp+1);
		addr = (uintptr_t) eip;
		cprintf("  ebp %08x eip %08x args",ebp, eip);
		for (i = 0;i < 5;++i) arg[i] = *(Ebp+i+2);
	//	cprintf("  ebp %08x eip %08x args",ebp, eip);
		for (i = 0;i < 5;++i)
			cprintf(" %08x",arg[i]);
		cprintf("\n");
		debuginfo_eip(addr, &info);
		cprintf("      %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
			   	info.eip_fn_namelen, info.eip_fn_name,
			   	(addr-info.eip_fn_addr));
		Ebp = (uint32_t *)(*Ebp);
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	if (tf != NULL)
		print_trapframe(tf);

	//cprintf("%C(8,4)Hello %C(2,7)every %C(6,8)one \n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
