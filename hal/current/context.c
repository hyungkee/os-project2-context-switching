#include <core/eos.h>
#include <core/eos_internal.h>

typedef struct _os_context {
	/* low address */
	int32u_t edi;
	int32u_t esi;
	int32u_t ebp;
//	int32u_t esp;
	int32u_t ebx;
	int32u_t edx;
	int32u_t ecx;
	int32u_t eax;
	int32u_t eflags;
	int32u_t eip;
	/* high address */	
} _os_context_t;

void print_context(addr_t context) {
	if(context == NULL) return;
	_os_context_t *ctx = (_os_context_t *)context;
	PRINT("edi  =0x%x\n", ctx->edi);
	PRINT("esi  =0x%x\n", ctx->esi);
	PRINT("ebp  =0x%x\n", ctx->ebp);
	PRINT("ebx  =0x%x\n", ctx->ebx);
	PRINT("edx  =0x%x\n", ctx->edx);
	PRINT("ecx  =0x%x\n", ctx->ecx);
	PRINT("ebx  =0x%x\n", ctx->ebx);
	PRINT("eax  =0x%x\n", ctx->eax);
	PRINT("eip  =0x%x\n", ctx->eip);
}

addr_t _os_create_context(addr_t stack_base, size_t stack_size, void (*entry)(void *), void *arg) {
	static int32u_t* sp;
	sp = stack_base;

	// push arg address
	*(--sp) = arg;

	// make function call convention things(old eip, old ebp);
	*(--sp) = 0; // old eip - set return addr(NULL)

	// make context
	sp -= sizeof(_os_context_t)/sizeof(int32u_t);

	// fill context
	((_os_context_t*)sp)->eip = entry;

	return sp;
}

void _os_restore_context(addr_t sp) {
	static int i;
	static _os_context_t context;

// stack pointer change
	__asm__ __volatile__ ("movl %0, %%esp;" ::"m"(sp));

// pop context
	for(i=0;i<sizeof(_os_context_t)/sizeof(int32u_t);i++)
		__asm__ __volatile__ ("\popl %0;" :"=r"(((int32u_t*)&context)[i]):);

// set context(without eip, ebp)
	__asm__ __volatile__ ("push %0; popf;" ::"m"(context.eflags)); // should be first
	__asm__ __volatile__ ("movl %0, %%edi;" ::"m"(context.edi));
	__asm__ __volatile__ ("movl %0, %%esi;" ::"m"(context.esi));
	__asm__ __volatile__ ("movl %0, %%eax;" ::"m"(context.eax));
	__asm__ __volatile__ ("movl %0, %%ebx;" ::"m"(context.ebx));
	__asm__ __volatile__ ("movl %0, %%ecx;" ::"m"(context.ecx));
	__asm__ __volatile__ ("movl %0, %%edx;" ::"m"(context.edx));

// trick for go to new eip & set ebp
	__asm__ __volatile__ ("push %0;" ::"m"(context.eip));
	__asm__ __volatile__ ("push %0;" ::"m"(context.ebp));

/* when function end, stream go to other stream. */
}


addr_t _os_save_context() {
	static int i;
	static addr_t ret;
// context box
	static _os_context_t context;
	static int32u_t* context_ptr;
	static int32u_t is_comeback;
	context_ptr = (int32u_t*)&context;

	__asm__ __volatile__ ("movl $0, %0;" :"=m"(is_comeback):); // is_comeback = 0;

// set context
// if we save context to *(context addr in stack) directly, some register have to save (context addr in stack). we have to save unmodulated register values, so should save regi to static variable area first(because static var's addr is constant).
	__asm__ __volatile__ ("movl %%eax, %0;" :"=r"(context.eax):);
	__asm__ __volatile__ ("movl %%ebx, %0;" :"=r"(context.ebx):);
	__asm__ __volatile__ ("movl %%ecx, %0;" :"=r"(context.ecx):);
	__asm__ __volatile__ ("movl %%edx, %0;" :"=r"(context.edx):);
	__asm__ __volatile__ ("movl %%ebp, %0;" :"=r"(context.ebp):);
	__asm__ __volatile__ ("movl %%esi, %0;" :"=r"(context.esi):);
	__asm__ __volatile__ ("movl %%edi, %0;" :"=r"(context.edi):);
	__asm__ __volatile__ ("pushf; pop %0;" :"=r"(context.eflags):);
	__asm__ __volatile__ ("lea get_addr, %0;" "get_addr:" : "=r"(context.eip));

/* when stream comeback, start from here. */
	__asm__ __volatile__ ("cmp $0, %0;" ::"m"(is_comeback));
	__asm__ __volatile__ ("movl $0, %%eax;" :); // set return value.. when I used 'return 0;' or 'ret = 0;' compiler remove this.. bad optimization :(
	__asm__ __volatile__ ("jne L1" :); // if is_comeback = 0, escape this function.

// it's same with this.. but... compiler delete this code during optimazation :(
//	if(is_comeback){
//		return 0;
//	}

	__asm__ __volatile__ ("movl $1, %0;" :"=r"(is_comeback):); // is_comeback = 1;

// push context (reverse order)
	for(i=sizeof(_os_context_t)/sizeof(int32u_t)-1;i>=0;i--){
		__asm__ __volatile__ ("pushl %0;" ::"r"(context_ptr[i]));
	}

// set eax(return)
	__asm__ __volatile__ ("movl %%esp, %%eax;":"=a"(ret));
	
// push (old)eip
	__asm__ __volatile__ ("movl 4(%%ebp), %%ebx;":);
	__asm__ __volatile__ ("pushl %%ebx;":);

// push (old)ebp
	__asm__ __volatile__ ("movl 0(%%ebp), %%ebx;":);
	__asm__ __volatile__ ("pushl %%ebx;":);

	__asm__ __volatile__ ("L1:":);
	return ret;
}

