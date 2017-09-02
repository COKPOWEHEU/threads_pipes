#include "multiproc.h"

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)

#define __USE_GNU
#include <fcntl.h>
#include <unistd.h>

unsigned int system_cpu_number(){
  long res = sysconf(_SC_NPROCESSORS_ONLN);
  if(res <= 0)return 1; //если посчитать не удалось - считаем одноядерным
  if(res > ((unsigned int)-1))return (unsigned int)-1; //если слишком много (как?!) выдаем максимум
  return res;
}

int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr){
  return pthread_create(&res->thread,(const pthread_attr_t *)NULL/*attr*/,func,param);
}

int mpthread_join(mpthread_t thread){
  return pthread_join(thread.thread, NULL);
}

char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd){
  int fd[2];
  if(pipe2(fd, O_NONBLOCK))return MTP_ERR;
  rd->fd = fd[0];
  wr->fd = fd[1];
  return MTP_OK;
}
unsigned long mppipe_read(mppipe_r rd, void *buf, size_t count){
  ssize_t res = read(rd.fd, buf, count);
  if(res<=0)return 0; //если канал пуст, read возвращает -1. Приведем к нулю
  return res;
}
unsigned long mppipe_write(mppipe_w wr, void *buf, size_t count){
  ssize_t res = write(wr.fd, buf, count);
  if(res<=0)return 0; //аналогично read
  return res;
}
char mppipe_close_write(mppipe_w *wr){
  return close(wr->fd);
}
char mppipe_close_read(mppipe_r *rd){
  return close(rd->fd);
}

#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

#include <stdio.h>
#include <stdlib.h>

unsigned int system_cpu_number(){
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  if(sysinfo.dwNumberOfProcessors > ((unsigned int)-1))return (unsigned int)-1;
  if(sysinfo.dwNumberOfProcessors == 0)return 1;
  return sysinfo.dwNumberOfProcessors;
}

int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr){
  res->thread = CreateThread((LPSECURITY_ATTRIBUTES)attr, 0, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
  return 0;
}
int mpthread_join(mpthread_t thread){
  WaitForSingleObject(thread.thread, INFINITE);
  CloseHandle(thread.thread);
  return 0;
}

//спасибо доблестным разработчикам Microsoft что не поддерживают нормальных анонимных каналов - приходится извращаться через именованные
char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd){
  char filename[256]; //ограничение WinAPI на размер имени файла в 256 символов
  //уменьшаем шанс наложения случайных имен каналов - в них будут ID процесса и потока
  DWORD tid = GetCurrentThreadId();
  DWORD pid = GetCurrentProcessId();
  //считаем, что каналы создаются прежде разделения на потоки, а раз так - ошибки не возникнет
  static DWORD count = 0xBAD-05; //bad OS, да
  //для страховки добавляем рандома
  if(count == 0xBAD-05)count = rand();
  //и, наконец, увеличиваем глобальный счетчик
  count++;
  //из всего предыдущего формируем имя канала
  snprintf(filename, 256, "\\\\.\\pipe\\multiproc_p%lu_t%lu_r%lu",pid, tid, count);

  rd->fd = CreateNamedPipe(filename,
                           PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
                           1, //max instances
                           0, //out buffer size
                           1, //in buffer size
                           1, //timeout, ms
                           NULL);
  if(rd->fd == INVALID_HANDLE_VALUE)return MTP_ERR;

  wr->fd = CreateFile(filename,
                      GENERIC_WRITE,
                      0, //share mode
                      NULL, //security
                      OPEN_EXISTING, //creation flags
                      FILE_FLAG_OVERLAPPED | FILE_FLAG_WRITE_THROUGH, //attribs
                      NULL);
  if(wr->fd == INVALID_HANDLE_VALUE){CloseHandle(rd->fd); return MTP_ERR;}
  
  ConnectNamedPipe(rd->fd, NULL);

  return MTP_OK;
}
unsigned long mppipe_read(mppipe_r rd, void *buf, size_t count){
  DWORD av_bytes, realcnt;
  //как пользоваться ReadFile / ReadFileEx чтобы он корректно отрабатывал пустой канал я не знаю
  //поэтому просто проверяем есть ли в канале хоть что-то
  PeekNamedPipe(rd.fd, NULL, 0, NULL, &av_bytes, NULL);
  if(av_bytes <= 0)return 0;
  ReadFile(rd.fd, buf, count, &realcnt, NULL);
  return realcnt;
}


unsigned long mppipe_write(mppipe_w wr, void *buf, size_t count){
  DWORD realcnt;
  WriteFile(wr.fd, buf, count, &realcnt, NULL);
  
  return realcnt;
}
char mppipe_close_write(mppipe_w *wr){
  CloseHandle(wr->fd);
  return 0;
}
char mppipe_close_read(mppipe_r *rd){
  CloseHandle(rd->fd);
  return 0;
}

#endif
