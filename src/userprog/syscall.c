#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"


// from file.c
struct file 
  { 
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

static void syscall_handler (struct intr_frame *);

struct lock sys_lock;
void
syscall_init (void) 
{
  lock_init(&sys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_user_address(void* addr){
  if(!is_user_vaddr(addr)){
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
      check_user_address(f->esp+4);
      check_user_address(f->esp+8);
      f->eax = create((const char *)*(uint32_t *)(f->esp+4), (unsigned)(*(uint32_t *)(f->esp+8)));
      break;
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
      check_user_address(f->esp+4);
      check_user_address(f->esp+8);
      check_user_address(f->esp+12);
      f->eax = read((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_WRITE:
      check_user_address(f->esp+4);
      check_user_address(f->esp+8);
      check_user_address(f->esp+12);
      f->eax = write((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_SEEK:
      check_user_address(f->esp+4);
      check_user_address(f->esp+8);
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

// Wait
int wait(pid_t pid){
  // Simply just call process_wait
  return process_wait(pid);
}

//Exit
void exit(int status){
  // when a process exits, it should print the exit message
  struct thread *cur = thread_current();

  printf("%s: exit(%d)\n", thread_name(), status);
  cur -> exit_status = status;
  int i = 3;
  while (i < 128) {
    // close all the files, when a process exits
    if (cur->fd[i] != NULL) {
        close(i);
    }
    i++;
  }
  thread_exit();
}

// Write
int write(int fd, const void *buffer, unsigned size){
  if (buffer == NULL)
    exit(-1);
  //check if the buffer is a valid user address
  check_user_address(buffer);
  check_user_address(buffer+size);

  struct file* fptr = thread_current()->fd[fd];

  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }
  
  
  if (fd>2) {
    if (fptr == NULL)
      exit(-1);
    if (fptr->deny_write) {
      file_deny_write(fptr);
    }
    lock_acquire(&sys_lock);
    int result = file_write(fptr, buffer, size);
    lock_release(&sys_lock);
    return result;
  }
  return -1;
}


int read (int fd, void* buffer, unsigned size){
  // reads 'size' bytes from the open file 'fd' into 'buffer'

  if(buffer == NULL)
    exit(-1);
  // check if the buffer is a valid user address
  check_user_address(buffer);
  check_user_address(buffer+size);  
  
  if (fd == 0){
    int i = 0;
    while (i < size) {
        ((char*)buffer)[i] = input_getc();
        if (((char*)buffer)[i] == '\0') {
            break;
        }
        i++;
    }
    return i;
  }
  
  if (fd>2){
    struct file* curr_file = thread_current()->fd[fd];

    if(curr_file == NULL){
      exit(-1);
    }

    lock_acquire(&sys_lock);
    int result = file_read(curr_file, buffer, size);
    lock_release(&sys_lock);
    return result;
  }

  return -1;

}

void halt(){
  // Simply just call shutdown_power_off for halt
  shutdown_power_off();
}

pid_t exec(const char *cmd_line){
  // Simply just call process_execute for exec
  return process_execute(cmd_line);
}

bool create (const char *file, unsigned initial_size){
  // Simply just call filesys_create
  if (file == NULL){
    exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool remove (const char *file){
  // Simply just call filesys_remove
  if (file == NULL)
    return false;
  return filesys_remove(file);
}

int open (const char *file){
  int start = 3;
  int max = 128;
  if (file == NULL){
    exit(-1);
  }

  struct file* open_file = filesys_open(file);
  struct thread* curr = thread_current();
  
  if (open_file == NULL){
    return -1;
  }

  int i = start;
  while (i < max) {
      if (curr->fd[i] == NULL) {
          if (strcmp(thread_current()->name, file) == 0) {
              file_deny_write(open_file);
          }   
          curr->fd[i] = open_file;
          return i;
      }
      i++;
  }

  return -1;
}

void close (int fd){
  struct file* curr_file = thread_current()->fd[fd];

  if (curr_file == NULL)
    exit(-1);
  file_close(curr_file);
  thread_current()->fd[fd] = NULL;
}

int filesize (int fd){
  struct file* curr_file = thread_current()->fd[fd];
  if (curr_file == NULL)
    exit(-1);
  return file_length(curr_file);
}

void seek (int fd, unsigned position){
  struct file* curr_file = thread_current()->fd[fd];
  if (curr_file == NULL)
    exit(-1);
  file_seek(curr_file, position);
}

unsigned tell (int fd){
  struct file* curr_file = thread_current()->fd[fd];
  if (curr_file == NULL)
    exit(-1);
  return file_tell(curr_file);
}