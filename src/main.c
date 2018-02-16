#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "multiproc.h"

/////////////////////////////////////////////////////////////////
// Test pipe
void test_pipe(){
  char buf[101];
  int len, i, sum = 0;
  
  mppipe_w pw;
  mppipe_r pr;
  
  mppipe_create_anon(&pw,&pr);

  for(i=0; i<10000; i++){
    snprintf(buf, 100, "ABCDEFGHIJKLMNOPQRSTUVWZYZАБВГДЕЖЗИКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ");
    len = mppipe_write(pw, buf, 50);
    sum++;
    printf("%i -> %i\n", sum, len);
    if(len == 0)break;
  }
  
  sum=0;
  for(i=0; i<10000; i++){
    sum++;
    memset(buf, 0, sizeof(buf));
    len = mppipe_read(pr, buf, 50);
    if(len == 0){printf("\nEOF %i\n", sum); break;}
    printf("\r%i bytes = '%s'",len, buf);
  }
  
  mppipe_close_read(&pr);
  mppipe_close_write(&pw);
}

/////////////////////////////////////////////////////////////////
// Test thread
struct file_io{
  mppipe_r pr;
  mppipe_w pw;
};
void* thread_func(void* arg){
  struct file_io f;
  char buf[101];
  int len, i;
  char tmp;
  memcpy(&f, arg, sizeof(struct file_io));
  
  pusleep(100 * 1000);
  
  len = mppipe_read(f.pr, buf, 100);
  if(len > 0){
    for(i=0; i<len/2; i++){
      tmp = buf[i];
      buf[i] = buf[len-i-1];
      buf[len-i-1] = tmp;
    }
  }else len = snprintf(buf, 100, "Error read (Thread)");
  
  mppipe_write(f.pw, buf, len);
  
  mppipe_close_write(&f.pw);
  mppipe_close_read(&f.pr);
  return 0;
}
void test_thread(){
  struct file_io fm; //files to Main
  struct file_io ft; //files to Thread
  mpthread_t thread;
  char buf[101];
  int len;
  
  mppipe_create_anon(&fm.pw, &ft.pr);
  mppipe_create_anon(&ft.pw, &fm.pr);
  mpthread_create(&thread, thread_func, &ft, NULL);
  
  snprintf(buf, 100, "0123456789");
  len = mppipe_write(fm.pw, buf, strlen(buf));
  printf("write %i bytes (Main) = '%s'\n", len, buf);
  
  pusleep(1000 * 1000);
  
  memset(buf, 0, sizeof(buf));
  len = mppipe_read(fm.pr, buf, 100);
  printf("%i bytes read (Main) = '%s'\n", len, buf);
  
  mpthread_join(thread);
  
  mppipe_close_write(&fm.pw);
  mppipe_close_read(&fm.pr);
}

//////////////////////////////////////////////////////////////////
// Test CPU number
void test_cpu(){
  unsigned int res = system_cpu_number();
  printf("%i CPUs\n", res);
}

///////////////////////////////////////////////////////////////////////////
//  Main
///////////////////////////////////////////////////////////////////////////
void disp_err(char *progname){
  printf("Using:\n");
  printf(" '%s -p' or '%s --pipe' : test pipe\n", progname, progname);
  printf(" '%s -t' or '%s --thread' : test thread\n", progname, progname);
  printf(" '%s -c' or '%s --cpu' : test CPU number\n", progname, progname);
}
int main(int argc, char *argv[]){
  if(argc != 2){
    disp_err(argv[0]);
    return 1;
  }
  
  if(strcmp(argv[1], "-p")==0 || strcmp(argv[1], "--pipe")==0){
    test_pipe();
  }else if(strcmp(argv[1], "-t")==0 || strcmp(argv[1], "--thread")==0){
    test_thread();
  }else if(strcmp(argv[1], "-c")==0 || strcmp(argv[1], "--cpu")==0){
    test_cpu();
  }else disp_err(argv[0]);
  
  return 0;
}
