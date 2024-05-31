#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

struct lock filesys_lock;

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_user_address(void* addr){

  if(!is_user_vaddr(addr)){
    exit(-1);
  }
}

void check_kernel_address(void* addr){
   if(!is_kernel_vaddr(addr)){
    exit(-1);
  }

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *number = f->esp;
  check_user_address(number);
  switch (*(uint32_t *)(number)) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_user_address(f->esp+4);
      exit((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_EXEC:
      check_user_address(f->esp+4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_WAIT:
      check_user_address(f->esp+4);
      f->eax = wait((pid_t)*(uint32_t *)(f->esp+4));
      break;   
    case SYS_CREATE:
      for(int i=1;i<=2;i++)
        check_user_address(f->esp+i*4);
      f->eax = create((const char *)*(uint32_t *)(f->esp+4), (unsigned)(*(uint32_t *)(f->esp+8)));
    case SYS_REMOVE:
      check_user_address(f->esp+4);
      f->eax = remove((const char *)*(uint32_t *)(f->esp+4));
      break;      
    case SYS_OPEN:
      check_user_address(f->esp+4);
      f->eax = open((const char *)*(uint32_t *)(f->esp+4));       
      break;
    case SYS_FILESIZE:
      check_user_address(f->esp+4);
      f->eax = filesize((int)(*(uint32_t *)(f->esp+4)));      
      break;
    case SYS_READ:
      for(int i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      f->eax = read((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_WRITE:
    // 주소에 들어있는 값을 가져오면 된다!! 내일 해결
      for(int i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      f->eax = write((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_SEEK:
      for(int i=1;i<=2;i++)
        check_user_address(f->esp+i*4);
      seek((int)(*(uint32_t *)(f->esp+4)), (unsigned)(*(uint32_t *)(f->esp+8)));
      break;
    case SYS_TELL:
      check_user_address(f->esp+4);
      f->eax = tell((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_CLOSE:
      check_user_address(f->esp+4);
      close((int)(*(uint32_t *)(f->esp+4)));
      break;
  }
}

int wait(pid_t pid){
  // Waits for a child process pid and retrieves the child's exit status.
  return process_wait(pid);
}


void exit(int status){
  struct thread *cur = thread_current();

  printf("%s: exit(%d)\n", thread_name(), status);
  cur -> exit_status = status;
  for(int i=3;i<128;i++){
    if(cur->fd[i]!=NULL){
      close(i);
    }
  }  
  thread_exit();
}


int write(int fd, const void *buffer, unsigned size){

  check_user_address(buffer);
  check_user_address(buffer+size);

  struct file* fptr = thread_current()->fd[fd];
  int return_val = -1;

  if(fd==1){
    putbuf(buffer, size);
    return size;
  }
  else if (fd>2) {
    if(fptr==NULL)
      exit(-1);
    if (fptr->deny_write) {
      file_deny_write(fptr);
    }
    lock_acquire(&filesys_lock);
    return_val = file_write(fptr, buffer, size);
    lock_release(&filesys_lock);
  }
  return return_val;
}


int read (int fd, void* buffer, unsigned size){

  // reads 'size' bytes from the open file 'fd' into 'buffer'
  int i = -1;

  if(buffer==NULL)
    exit(-1);

  check_user_address(buffer);
  check_user_address((void*)((uint32_t)buffer+size));  
  
  if (fd==0){
    for (i = 0; i < size; i++)
	  {
	    ((char*)buffer)[i] = input_getc();
      if(((char*)buffer)[i] =='\0')
        break;
	  }
    return i;
  }
  else if (fd>2){
    if(thread_current()->fd[fd]==NULL){
      return -1;
    }
    lock_acquire(&filesys_lock);
    i = file_read(thread_current()->fd[fd], buffer, size);
    lock_release(&filesys_lock);
  }
  return i;

}

void halt(){
  shutdown_power_off();
}

pid_t exec(const char *cmd_line){
  return process_execute(cmd_line);
}

bool create (const char *file, unsigned initial_size){
  if(file==NULL){
    exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool remove (const char *file){
  if(file==NULL)
    return false;
  return filesys_remove(file);
}

int open (const char *file){

  if(file==NULL){
    return -1;
  }

  struct file* f = filesys_open(file);
  struct thread* t = thread_current();
  
  if(f==NULL){
    return -1;
  }

  int i;
  for(i=3;i<128;i++){
    if(t->fd[i]==NULL){
      if (strcmp(thread_current()->name, file) == 0) {
            file_deny_write(f);
        }   
      t->fd[i] = f;
      return i;
    }
  }

  return -1;
}

void close (int fd){
  if(thread_current()->fd[fd]==NULL)
    exit(-1);
  file_close(thread_current()->fd[fd]);
  thread_current()->fd[fd] = NULL;
}

int filesize (int fd){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  return file_length(f);
}

void seek (int fd, unsigned position){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  file_seek(f, position);
}

unsigned tell (int fd){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  return file_tell(f);
}