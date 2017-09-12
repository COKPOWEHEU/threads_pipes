#ifndef _MULTIPROC_H_
#define _MULTIPROC_H_
#include "portability.h"

//совместимость 
#ifdef __cplusplus
extern "C" {
#endif

#define MTP_OK  0
#define MTP_ERR 1

#if defined(linux)
  #include <pthread.h>
  typedef union {pthread_t thread;}mpthread_t; //дескриптор потока //thread descriptor
#elif defined(WIN32)
  #include <windows.h>
  //можно было использовать POSIX реализацию, но она тянет за собой libwinpthread-1.dll
  //зачем оно когда и родные Win32-потоки работают нормально
  typedef union {HANDLE thread;}mpthread_t;
  #define MPPIPE_BYPASS
#else
  #error architecture does not supported yet
#endif
  typedef union {void *attr;}mpthread_arrt_t; //атрибуты при создании потока (не используется) //thread attributes (ignore yet)
  
#ifdef MPPIPE_BYPASS
/*читать - КОСТЫЛЬ. Ручное создание FIFO буфера и работа с ним
  это менее отлаженный вариант. Наверняка более медленный, зато, наверное, более переносимый
  Но в целом - если есть нормальные каналы (не тот ужас, что считается каналами в WinAPI) лучше использовать их
*/
  typedef union {void *fifo;}mppipe_w; //дескриптор канала на запись
  typedef union {void *fifo;}mppipe_r; //дескриптор канала на чтение
#elif defined(linux)
  typedef union {int fd;}mppipe_w; //write descriptor of pipe
  typedef union {int fd;}mppipe_r; //read descriptor of pipe
#else
  #error normal pipes not supported for this architecture. Try to define MPPIPE_BYPASS
#endif
  
//получить количество ядер/процессоров
//return number of processors
unsigned int system_cpu_number();

//создание нового потока. Результат 0 - успех, остальное - ошибки
//create new thread. Return value: 0 - success, other - errors
int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr);
//ожидание завершения потока. Результат 0 - успех, остальное - ошибки
//wait for finishing of thread. Return value: 0 - success, other - errors
int mpthread_join(mpthread_t thread);
  
//создание анонимного канала. Результат MTP_ERR - ошибка, MTP_OK - успех
//create anonymous pipe. Return value: MTP_OK - success, MTP_ERR - error
char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd);
//чтение и запись в канал. buf-откуда читать/писать, count-сколько байт. Результат - сколько реально прочитано/записано
//если операция невозможна - функция не блокируется, возвращаемое значение 0
//read and write into pipe. rd/wd - pipe descriptor, buf - data to read/write, count - number of bytes
//return value - number ob bytes actually read/write. If pipe empty/full, function does not block
unsigned long mppipe_read(mppipe_r rd, void *buf, size_t count);
unsigned long mppipe_write(mppipe_w wr, void *buf, size_t count);
//закрытие канала
//close pipe
char mppipe_close_write(mppipe_w *wr);
char mppipe_close_read(mppipe_r *rd);

#ifdef __cplusplus
}
#endif

#endif
