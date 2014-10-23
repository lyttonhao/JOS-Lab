// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int c;

	sys_env_set_priority(0, ENV_PRIOR_HIGH);

	if ((c = fork()) != 0)
	{
		sys_env_set_priority(c, ENV_PRIOR_LOW);
		if ((c = fork()) != 0)
		{
			sys_env_set_priority(c, ENV_PRIOR_DEFAULT);
		}
	}

	struct Env *now;
	
	now = (struct Env*)envs + ENVX(sys_getenvid());
	
	int i;
	for (i = 0;i < 3;++i)
	{
		cprintf("Env %x: My priority is %d\n", sys_getenvid(), 
				now->env_priority);
		sys_yield();
	}


}

