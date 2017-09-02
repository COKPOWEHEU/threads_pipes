#ifndef _MULTIPROC_H_
#define _MULTIPROC_H_

//совместимость 
#ifdef __cplusplus
extern "C" {
#endif

#define MTP_OK  0
#define MTP_ERR 1

#if (defined(linux))
  #include <pthread.h>
  typedef union {int fd;}mppipe_w; //дескриптор канала на запись
  typedef union {int fd;}mppipe_r; //дескриптор канала на чтение
  typedef union {pthread_t thread;}mpthread_t; //дескриптор потока
  typedef union {void *attr;}mpthread_arrt_t; //атрибуты при создании потока (не используется)
#elif (defined(WIN32))
  #include <windows.h>
  typedef union {HANDLE fd;}mppipe_w; //FIFO write descriptor
  typedef union {HANDLE fd;}mppipe_r; //FIFO read descriptor
  typedef union {HANDLE thread;}mpthread_t; //thread descriptor
  typedef union {void *attr;}mpthread_arrt_t; //thread attributes (not used yet)
#else
  #error architecture does not supported yet
#endif
  
//получить количество ядер/процессоров (если не удалось - 1, т.к. хоть один процессор точно есть)
unsigned int system_cpu_number();

//создание нового потока. Результат 0 - успех, остальное - ошибки
int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr);
//ожидание завершения потока. Результат 0 - успех, остальное - ошибки
int mpthread_join(mpthread_t thread);
  
//создание анонимного канала. Результат MTP_ERR - ошибка, MTP_OK - успех
//create anonymous pipe. Return value: MTP_ERR - error, MTP_OM - success
char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd);

//чтение и запись в канал. buf-откуда читать/писать, count-сколько байт. Результат - сколько реально прочитано/записано
//если операция невозможна - функция не блокируется, возвращаемое значение 0
//read and write into pipe. buf - data to read/write, count - number of bytes to read/write. Return value - number of bytes really read/written 
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
