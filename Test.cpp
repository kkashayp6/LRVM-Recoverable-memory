#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "rvm.h"

/* proc1 writes some data, commits it, then exits */

void proc1() 
{
  rvm_t rvm;
  trans_t trans;
  char* segs[1];

  rvm = rvm_init("rvm_segments");
  rvm_destroy(rvm, "testseg");
  segs[0] = (char *) rvm_map(rvm, "testseg", 10);
     
  trans = rvm_begin_trans(rvm, 1, (void **) segs);
  rvm_about_to_modify(trans, segs[0], 0, 5);
  sprintf(segs[0], "foo");
  rvm_commit_trans(trans);
  
  trans = rvm_begin_trans(rvm, 1, (void **) segs);
  sprintf(segs[0], "boo");
  rvm_commit_trans(trans);
  
  trans = rvm_begin_trans(rvm, 1, (void **) segs);
  rvm_about_to_modify(trans, segs[0], 4, 5);
  sprintf(segs[0]+4, "ai");
  rvm_commit_trans(trans);

  abort();
}


/* proc2 opens the segments and reads from them */
void proc2() 
{
  char* segs[1];
  rvm_t rvm;
     
  rvm = rvm_init("rvm_segments");

  segs[0] = (char *) rvm_map(rvm, "testseg", 10);
  if(strcmp(segs[0], "foo")) {
    fprintf(stderr, "A second process did not find what the first had written1.\n");
    exit(EXIT_FAILURE);
  }
  if(strcmp(segs[0]+4, "ai")) {
    fprintf(stderr, "A second process did not find what the first had written2.\n");
    exit(EXIT_FAILURE);
  }
}


int main(int argc, char **argv)
{
  int pid;
  pid = fork();
  if(pid < 0) {
  perror("fork");
  exit(2);
  }
  if(pid == 0) 
  {
    proc1();
    exit(EXIT_SUCCESS);
  }

  waitpid(pid, NULL, 0);

  proc2();

  printf("Ok\n");

  return 0;
}
