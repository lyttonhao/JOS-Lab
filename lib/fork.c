// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pt;
	
	if ( (err & FEC_WR) == 0 ) panic("panic pgfault:not FEC_WR\n");

	pt = (pte_t ) uvpt[ ((uint32_t)addr) >> PGSHIFT ];
	if ( (pt & PTE_P) == 0 || (pt & PTE_COW) == 0) panic("pgault panic!\n");
	pt = (pte_t ) uvpd[ PDX(addr) ];
	if ( (pt & PTE_P) == 0 )                   //nonono
		panic("pgfault panic!\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_W | PTE_U);
	if (r < 0) panic("panic pgfault: %e\n", r);
	
	addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, addr, PGSIZE);

	sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W);

//	panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r, r1;
	void *addr = (void *)(pn*PGSIZE);
	pte_t pt;
	// LAB 4: Your code here.
	//panic("duppage not implemented");
	pt = (pte_t )(uvpt[pn]);

	if ((pt & PTE_W) > 0 || (pt & PTE_COW) > 0)
	{
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_COW | PTE_P);
		r1 = sys_page_map(0, addr, 0, addr, PTE_U | PTE_COW | PTE_P);
	}
	else {
			r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P);
			r1 = 0;
		 } 	
	if (r < 0) panic("duppage panic: %e\n", r);
	if (r1 < 0) panic("duppage panic: %e\n", r1);

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
//	panic("fork not implemented");
	
	envid_t envid;
	uintptr_t addr;
	int r;
	extern void _pgfault_upcall(void);


	set_pgfault_handler( pgfault );
	envid = sys_exofork();
	if (envid < 0)
		panic("fork panic: %e\n", envid);

	if (envid == 0)    //child env
	{
		
		thisenv = envs + ENVX( sys_getenvid() );

		return 0;
	}

	for (addr = 0; addr < UXSTACKTOP-PGSIZE;addr += PGSIZE)
	   if ((uvpd[PDX(addr)] & PTE_P) > 0 && (uvpd[PDX(addr)] & PTE_U) > 0 &&
			(uvpt[((uint32_t)addr) >> PGSHIFT] & PTE_P ) > 0 
					&& (uvpt[((uint32_t)addr) >> PGSHIFT] & PTE_U) > 0)
		{
			duppage(envid, addr/PGSIZE);
		}
	r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), 
							PTE_U | PTE_W | PTE_P);
	if (r < 0) panic("fork panic: %e\n", r);

	sys_env_set_pgfault_upcall(envid, (void *)_pgfault_upcall);

	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0) panic("fork panic: %e\n", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
