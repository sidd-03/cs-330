#include <context.h>
#include <memory.h>
#include <lib.h>
#include <entry.h>
#include <file.h>
#include <tracer.h>

///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////
// 0 from write function ; 1 from read function
int is_valid_mem_range(unsigned long buff, u32 count, int access_bit)
{
	struct exec_context *current = get_current_ctx();

	struct vm_area *vm = current->vm_area;

	if (buff >= current->mms[MM_SEG_CODE].start && buff + count - 1 <= current->mms[MM_SEG_CODE].next_free - 1)
	{
		// printk("code area\n");
		if (access_bit == 0)
			return 1;
		else
			return 0;
	}
	else if (buff >= current->mms[MM_SEG_RODATA].start && buff + count - 1 <= current->mms[MM_SEG_RODATA].next_free - 1)
	{
		// printk("rodata\n");
		if (access_bit == 0)
			return 1;
		else
			return 0;
	}
	else if (buff >= current->mms[MM_SEG_DATA].start && buff + count - 1 <= current->mms[MM_SEG_DATA].next_free - 1)
	{
		// printk("segdata\n");
		return 1;
	}
	else if ((buff >= current->mms[MM_SEG_STACK].start && buff + count - 1 <= current->mms[MM_SEG_STACK].end - 1) || (buff <= current->mms[MM_SEG_STACK].start && buff + count - 1 >= current->mms[MM_SEG_STACK].end - 1))
	{
		//	printk("stack\n");
		return 1;
	}
	else if (vm != NULL)
	{

		while (vm != NULL)
		{
			if (buff >= vm->vm_start && buff + count - 1 <= vm->vm_end - 1)
			{
				// printk("vmarea\n");
				if (access_bit == 0)
				{
					if (vm->access_flags%2)
						return 1;
					else
						return 0;
				}
				else
				{
					if ((vm->access_flags/2)%2)
						return 1;

					else
						return 0;
				}
			}
			vm = vm->vm_next;
		}
	}
	else
	{
		// printk("not in any seg\n");
		return 0;
	}
}

long trace_buffer_close(struct file *filep)
{
	os_free(filep->fops, sizeof(struct fileops));
	os_page_free(USER_REG, filep->trace_buffer->buffer);
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	os_free(filep, sizeof(struct file));

	filep = NULL;

	return 0;
}

int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
		if(filep->mode!= O_RDWR && filep->mode!= O_READ) return -EINVAL;
	if (is_valid_mem_range((unsigned long)buff, count, 1) == 0)
		return -EBADMEM;

	
	int readp = filep->trace_buffer->read;
	int writep = filep->trace_buffer->write;
	int max_bytes_left = filep->trace_buffer->size;
	int i = readp, bytes_read = 0;
	// if (filep->trace_buffer->empty)
	// 	return 0;
	//printk("max bytes left: %d\n", max_bytes_left);
	while (count != 0 && bytes_read < max_bytes_left)
	{
		buff[bytes_read] = filep->trace_buffer->buffer[i];
		i = (i + 1) % TRACE_BUFFER_MAX_SIZE;
		bytes_read++;
		count--;
	 filep->trace_buffer->size--;
	}

	filep->trace_buffer->read = i;
	readp = filep->trace_buffer->read;
	if (readp == writep)
		filep->trace_buffer->empty = 1;
//	printk("read end =%d  write end  =%d\n", readp, writep);
	return bytes_read;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{ 
	if(filep->mode!= O_RDWR && filep->mode!= O_WRITE) return -EINVAL;
	if (is_valid_mem_range((unsigned long)buff, count, 0) == 0)
		return -EBADMEM;
	
	struct trace_buffer_info *trace = filep->trace_buffer;
	int readp = filep->trace_buffer->read;
	int writep = filep->trace_buffer->write;
	
	int max_bytes_left= 4096-filep->trace_buffer->size;
	int i = writep, bytes_written = 0;

	//	printk("Yo\n");
	// if (!filep->trace_buffer->empty && (readp == writep))
	// 	return 0;
	while (count != 0 && bytes_written < max_bytes_left)
	{
		filep->trace_buffer->buffer[filep->trace_buffer->write++] = buff[bytes_written++];
		filep->trace_buffer->write %= TRACE_BUFFER_MAX_SIZE;
		count--;
		filep->trace_buffer->size++;

	}
	// printk("Nice\n");
	//  filep->trace_buffer->write = bytes_written + writep;
	if (bytes_written > 0)
		filep->trace_buffer->empty = 0;
	return bytes_written;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	if(current == NULL) return -EINVAL;
	if(mode != O_RDWR && mode!=O_WRITE && mode!=O_READ) return -EINVAL;
	int free_fd = -1;
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (current->files[i] == NULL)
		{
			free_fd = i;
			break;
		}
	}
	if (free_fd == -1)
		return -EINVAL;

	struct file *trace_buffer = (struct file *)os_alloc(sizeof(struct file));
	if (trace_buffer == NULL)
		return -ENOMEM;


    if(current->files==NULL) return -EINVAL;

	current->files[free_fd] = trace_buffer;

	trace_buffer->type = TRACE_BUFFER;
	trace_buffer->mode = mode;
	trace_buffer->offp = 0;
	trace_buffer->ref_count = 1;
	trace_buffer->inode = NULL;
	trace_buffer->trace_buffer = (struct trace_buffer_info *)os_alloc(sizeof(struct trace_buffer_info));
	if (trace_buffer->trace_buffer == NULL)
		return -ENOMEM;

	trace_buffer->trace_buffer->buffer = (char *)os_page_alloc(USER_REG);
	if (trace_buffer->trace_buffer->buffer == NULL)
		return -ENOMEM;

	trace_buffer->trace_buffer->mode = mode;
	trace_buffer->trace_buffer->read = 0;
	trace_buffer->trace_buffer->write = 0;
	trace_buffer->trace_buffer->empty = 1;
	trace_buffer->trace_buffer->size=0;
	trace_buffer->fops = (struct fileops *)os_alloc(sizeof(struct fileops));
	if (trace_buffer->fops == NULL)
		return -ENOMEM;

	trace_buffer->fops->read = trace_buffer_read;
	trace_buffer->fops->write = trace_buffer_write;
	trace_buffer->fops->close = trace_buffer_close;
	trace_buffer->fops->lseek = 0;

	return free_fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int arg_num(u64 syscall_num)
{

	if (syscall_num == 1)
		return 1;
	else if (syscall_num == 2)
		return 0;
	else if (syscall_num == 4)
		return 2;
	else if (syscall_num == 5)
		return 3;
	else if (syscall_num == 6)
		return 1;
	else if (syscall_num == 7)
		return 1;
	else if (syscall_num == 8)
		return 2;
	else if (syscall_num == 9)
		return 2;
	else if (syscall_num == 10)
		return 0;
	else if (syscall_num == 11)
		return 0;
	else if (syscall_num == 12)
		return 1;
	else if (syscall_num == 13)
		return 0;
	else if (syscall_num == 14)
		return 1;
	else if (syscall_num == 15)
		return 0;
	else if (syscall_num == 16)
		return 4;
	else if (syscall_num == 17)
		return 2;
	else if (syscall_num == 18)
		return 3;
	else if (syscall_num == 19)
		return 1;
	else if (syscall_num == 20)
		return 0;
	else if (syscall_num == 21)
		return 0;
	else if (syscall_num == 22)
		return 0;
	else if (syscall_num == 23)
		return 2;
	else if (syscall_num == 24)
		return 3;
	else if (syscall_num == 25)
		return 3;
	else if (syscall_num == 27)
		return 1;
	else if (syscall_num == 28)
		return 2;
	else if (syscall_num == 29)
		return 1;
	else if (syscall_num == 30)
		return 3;
	else if (syscall_num == 35)
		return 4;
	else if (syscall_num == 36)
		return 1;
	else if (syscall_num == 37)
		return 2;
	else if (syscall_num == 38)
		return 0;
	else if (syscall_num == 39)
		return 3;
	else if (syscall_num == 40)
		return 2;
	else if (syscall_num == 41)
		return 3;
	else if (syscall_num == 61)
		return 0;
	else
		return -1;
}

int strace_write(struct file *filep, char *buff, u32 count)
{
  //  printk("into write\n");
	if(filep->mode!= O_RDWR && filep->mode!= O_WRITE) return -EINVAL;
	
	//    char * t_buff = filep->trace_buffer->buffer;
	struct trace_buffer_info *trace = filep->trace_buffer;
	int readp = filep->trace_buffer->read;
	int writep = filep->trace_buffer->write;
	// int max_bytes_left = 1 + (TRACE_BUFFER_MAX_SIZE - writep + readp - 1) % TRACE_BUFFER_MAX_SIZE;
	int max_bytes_left= 4096-filep->trace_buffer->size;
	//printk("max bytes left to write: %d\n", max_bytes_left);
	int i = writep, bytes_written = 0;
    
	//	printk("Yo\n");
	// if (!filep->trace_buffer->empty && (readp == writep))
	// 	return 0;
	while (count != 0 && bytes_written < max_bytes_left)
	{
		filep->trace_buffer->buffer[filep->trace_buffer->write++] = buff[bytes_written++];
		filep->trace_buffer->write %= TRACE_BUFFER_MAX_SIZE;
		count--;
		filep->trace_buffer->size++;

	}
	// printk("Nice\n");
	//  filep->trace_buffer->write = bytes_written + writep;
	if (bytes_written > 0)
		filep->trace_buffer->empty = 0;
	//	printk("bytes written: %d\n", bytes_written);
	return bytes_written;
}

int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	if (syscall_num == 37 || syscall_num == 38 || syscall_num == 1)
		return 0;
	struct exec_context *current = get_current_ctx();
	struct strace_info *find = NULL;
	if (current->st_md_base->is_traced == 0)
		return 0;
	if (current->st_md_base->tracing_mode == FILTERED_TRACING)
	{
		find = current->st_md_base->next;
		while (find != NULL)
		{
			if (find->syscall_num == syscall_num)
				break;
			find = find->next;
		}

		if (find == NULL)
			return 0;
	}

	int n = arg_num(syscall_num);
	if (n == -1)
		return 0;
	int fd = current->st_md_base->strace_fd;

	if (fd < 0)
		return 0;

	u64 m[] = {syscall_num, param1, param2, param3, param4};
	u64 buffer[n + 1];
	for (int i = 0; i <= n; i++)
	{
		buffer[i] = m[i];

		// printk("in perform arg %d: %x\n",i+1, buffer[i]);
	}

	u32 count = 8 * n + 8;
	strace_write(current->files[fd], (char *)buffer, count);
	return 0;
}

int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	if (current == NULL)
		return -EINVAL;

	if (arg_num(syscall_num) < 0)
		return -EINVAL;
	if (current->st_md_base == NULL)
	{
		current->st_md_base = (struct strace_head *)os_alloc(sizeof(struct strace_head));
		if (current->st_md_base == NULL)
			return -EINVAL;

		current->st_md_base->next == NULL;
		current->st_md_base->last == NULL;
		current->st_md_base->count = 0;
		current->st_md_base->is_traced = 0;
		current->st_md_base->tracing_mode = 0;
	}

	if (action == ADD_STRACE && current->st_md_base->count == STRACE_MAX)
		return -EINVAL;

	if (action == ADD_STRACE && current->st_md_base->count < STRACE_MAX)
	{
		struct strace_info *find = current->st_md_base->next;
		int flag=0;
		while (find != NULL)
		{
			if (find->syscall_num == syscall_num){
				flag=1;
				break;
			}
				
			find = find->next;
		}
		if(flag) -EINVAL; 

		struct strace_info *new = (struct strace_info *)os_alloc(sizeof(struct strace_info));
		if (new == NULL)
			return -EINVAL;

		new->next = NULL;
		new->syscall_num = syscall_num;
		if (current->st_md_base->next == NULL)
			current->st_md_base->next = new;
		if (current->st_md_base->last == NULL)
			current->st_md_base->last = new;
		else
		{
			current->st_md_base->last->next = new;
			current->st_md_base->last = new;
		}
		current->st_md_base->count++;
	}
	else if (action == REMOVE_STRACE)
	{
		if (current->st_md_base->next == NULL || current->st_md_base == NULL)
			return -EINVAL;

		if (current->st_md_base->next->syscall_num == syscall_num)
		{
			current->st_md_base->next = current->st_md_base->next->next;
			return 0;
		}
		if (current->st_md_base->last->syscall_num == syscall_num)
		{
			current->st_md_base->last = NULL;
			return 0;
		}

		else
		{

			struct strace_info *remove = current->st_md_base->next;

			while (remove->next != NULL)
			{

				if (remove->next->syscall_num == syscall_num)
				{
					remove->next = remove->next->next;
					break;
				}
				remove = remove->next;
			}
			if (remove == NULL)
				return -EINVAL;
		}
		current->st_md_base->count--;
	}

	else
		return -EINVAL;

	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	int total = 0;
	int r = 0;
	for (int i = 0; i < count; i++)
	{

		r = trace_buffer_read(filep, buff + total, 8);

		u64 sys = buff[total];
		int n = arg_num(sys);
		if (n == -1)
			return total;
		total += r;

		//	printk("sys: %d\n", sys);
		for (int j = 0; j < n; j++)
		{
			total += trace_buffer_read(filep, buff + total, 8);
			//  printk("arg %d: %x\n",j+1, buff[total-8]);
		}
	}

	return total;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{

	if (current == NULL)
		return -EINVAL;
	if (current->st_md_base == NULL)
	{
		current->st_md_base = (struct strace_head *)os_alloc(sizeof(struct strace_head));
		if (current->st_md_base == NULL)
			return -EINVAL;
		current->st_md_base->next == NULL;
		current->st_md_base->last == NULL;
		current->st_md_base->count = 0;
	}

	current->st_md_base->is_traced = 1;
	current->st_md_base->strace_fd = fd;
	current->st_md_base->tracing_mode = tracing_mode;

	return 0;
}

int sys_end_strace(struct exec_context *current)
{
	current->st_md_base->is_traced = 0;
	//	current->st_md_base->tracing_mode = -1;

	while (current->st_md_base->next != NULL)
	{
		struct strace_info *curr = current->st_md_base->next;
		current->st_md_base->next = current->st_md_base->next->next;
		os_free(curr, sizeof(struct strace_info));
	}
	current->st_md_base->last = NULL;
	current->st_md_base->count = 0;

	return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
	//printk("do ftrace\n");

	if (action == ADD_FTRACE)
	{

		//printk("add: do ftrace\n");

		if (ctx->ft_md_base == NULL)
		{
			ctx->ft_md_base = (struct ftrace_head *)os_alloc(sizeof(struct ftrace_head));
			if (ctx->ft_md_base == NULL)
				return -EINVAL;

			ctx->ft_md_base->next == NULL;
			ctx->ft_md_base->last == NULL;
			ctx->ft_md_base->count = 0;
		}

		if (ctx->ft_md_base->count == FTRACE_MAX)
			return -EINVAL;

		struct ftrace_info *find = ctx->ft_md_base->next;
		int flag =0;
		while (find != NULL)
		{
			if (find->faddr == faddr){
				flag =1;
				break;
			}
				
			find = find->next;
		}

		if(flag) return -EINVAL;
		
		struct ftrace_info *new = (struct ftrace_info *)os_alloc(sizeof(struct ftrace_info));
		if (new == NULL)
			return -EINVAL;

		new->next = NULL;
		new->faddr = faddr;
		new->num_args = nargs;
		new->fd = fd_trace_buffer;

		if (ctx->ft_md_base->next == NULL)
			ctx->ft_md_base->next = new;
		if (ctx->ft_md_base->last == NULL)
			ctx->ft_md_base->last = new;
		else
		{
			ctx->ft_md_base->last->next = new;
			ctx->ft_md_base->last = new;
		}
		ctx->ft_md_base->count++;

		//printk("added: do ftrace\n");
		return 0;
	}

	else if (action == REMOVE_FTRACE)
	{
		//printk("remove: do ftrace\n");
		if (ctx->ft_md_base->next == NULL || ctx->ft_md_base == NULL)
			return -EINVAL;

		if (ctx->ft_md_base->next->faddr == faddr)
		{
			ctx->ft_md_base->next = ctx->ft_md_base->next->next;
		}
		if (ctx->ft_md_base->last->faddr == faddr)
		{
			ctx->ft_md_base->last = NULL;
		}

		else
		{

			struct ftrace_info *remove = ctx->ft_md_base->next;

			while (remove->next != NULL)
			{

				if (remove->next->faddr == faddr)
				{
					remove->next = remove->next->next;
					break;
				}
				remove = remove->next;
			}
			if (remove == NULL)
				return -EINVAL;
		}
		ctx->ft_md_base->count--;
		return 0;
	}
	else
	{
		struct ftrace_info *find = ctx->ft_md_base->next;
		while (find != NULL)
		{

			if (find->faddr == faddr)
				break;
			find = find->next;
		}

		if (find == NULL)
			return -EINVAL;

		u8 *change = (u8 *)faddr;

		if (action == ENABLE_FTRACE)
		{  if(change[0]==INV_OPCODE) return 0;
		//	printk("enable: do ftrace\n");
			for (int i = 0; i < 4; i++)
			{
				find->code_backup[i] = change[i];
				change[i] = INV_OPCODE;
			}

			return 0;
		}
		else if (action == DISABLE_FTRACE)
		{
			if(change[0]!=INV_OPCODE) return 0;
		//	printk("disable: do ftrace\n");
			for (int i = 0; i < 4; i++)
			{
				change[i] = find->code_backup[i];
			}

			return 0;
		}
		else if (action == ENABLE_BACKTRACE)
		{
		//	printk("enable back: do ftrace\n");
			if(change[0]==INV_OPCODE) ;
		
			else{
				for (int i = 0; i < 4; i++)
			{
				find->code_backup[i] = change[i];
				change[i] = INV_OPCODE;
			}
			}
			find->capture_backtrace = 1;
			

			return 0;
			
		}
		else if (action == DISABLE_BACKTRACE)
		{
			//printk("disable back: do ftrace\n");
			for (int i = 0; i < 4; i++)
			{
				change[i] = find->code_backup[i];
			}
			find->capture_backtrace = 0;
			return 0;
		}
		else
		{
		//	printk("no ftrace\n");
			return -EINVAL;
		}
	}
}

// Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
	// printk("Handler\n");
	struct exec_context *ctx = get_current_ctx();
	//  printk("base: %x\n",ctx->ft_md_base);
	//  printk("base next: %x\n",ctx->ft_md_base->next);

	if (ctx->ft_md_base->next == NULL || ctx->ft_md_base == NULL)
		return -EINVAL;
   // printk("Handler 1\n");
	struct ftrace_info *find = ctx->ft_md_base->next;
	while (find != NULL)
	{
		//  printk("faddr: %x, entryip: %x\n",find->faddr,regs->entry_rip);
		if (find->faddr == regs->entry_rip)
			break;
		find = find->next;
	}

	if (find == NULL)
		return -EINVAL;
	//	 printk("Handler 2\n");

	regs->entry_rsp = regs->entry_rsp - 8;
	*((u64 *)regs->entry_rsp) = regs->rbp;
	regs->rbp = regs->entry_rsp;
	regs->entry_rip = regs->entry_rip + 4;

	int n = find->num_args;

	u64 buffer[n + 1];
	u64 m[] = {find->faddr, regs->rdi, regs->rsi, regs->rdx, regs->rcx, regs->r8, regs->r9};

	for (int i = 0; i < n + 1; i++)
	{

		buffer[i] = m[i];
		// printk("write: %x\n", buffer[i] );
	}
	int fd = find->fd;
	if (fd < 0)
		return -EINVAL;
	//	 printk("Handler 3\n");
	u32 count = 8 * n + 8;
	int strace_wr = strace_write(ctx->files[fd], (char *)buffer, count);
	
	u64 store[1];

	if (find->capture_backtrace == 0)
	{
		store[0] = 0xFFFFFFFFFFFFFFFF;
	strace_wr+=	strace_write(ctx->files[fd], (char *)store, 8);
	//printk("strace write: %d\n", strace_wr);
		// printk("write: %x\n", store[0] );
		// printk("handler exit\n");
		return 0;
	}
	u64 sp = regs->entry_rsp;
	u64 rbp = regs->rbp;

	store[0] = find->faddr;

	if (find->capture_backtrace)
	{
		strace_write(ctx->files[fd], (char *)store, 8);

		while (*((u64 *)(rbp + 8)) != END_ADDR)
		{
			store[0] = *((u64 *)(rbp + 8));
			strace_write(ctx->files[fd], (char *)store, 8);
			rbp = *((u64 *)rbp);
		}
		store[0] = 0xFFFFFFFFFFFFFFFF;
	  	strace_write(ctx->files[fd], (char *)store, 8);
	}
	//  printk("handler exit\n");
	return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
		int total = 0;
		struct exec_context * ctx = get_current_ctx();

		 int r=0;
	
		 for(int i=0; i<count ; i++){
			r = trace_buffer_read(filep,buff+total,8);
			if(r==0) return total;
			u64 val=((u64*)buff)[total];
			    
	            total+=8;
              // printk("buffer is %d \n", val);
			  while(val != 0xFFFFFFFFFFFFFFFF){
			   
			    r = trace_buffer_read(filep,buff+total,8);
				if(r==0) return total;
				if(r<8) return -EINVAL;
				total+= 8;
				// printk("ftrace total read: %d\n",total);
				// printk("buff[%d] :%x \n",(total-8)/8, buff[total-8]);
				
				val=buff[total-8];
				
			}
			 total-=8;

			 //printk("buff[%d] :%x \n",(total)/8, buff[total]);
			buff[total]=0;
			// printk("buff[%d] :%x \n",(total)/8, buff[total]);
			 
			

		 }

	 return total;

	
}
