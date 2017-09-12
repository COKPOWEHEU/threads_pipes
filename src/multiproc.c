#include "multiproc.h"

#define MAX_CPU 1024 /* нефиг требовать больше */

#if (defined(linux))
/**********************************************************************************
 *   LINUX
 **********************************************************************************
 */
#define __USE_GNU
#include <fcntl.h>
#include <unistd.h>
//не знаю что тут комментировать - все слишком очевидно
unsigned int system_cpu_number(){
  long res = sysconf(_SC_NPROCESSORS_ONLN); //говорят, это работает не везде, но ручной парсинг /proc/cpuinfo еще хуже
  if(res <= 0)return 1; //если посчитать не удалось - считаем одноядерным
  if(res > MAX_CPU)return MAX_CPU; //если слишком много (как?!) выдаем максимум
  return res;
}

int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr){
  return pthread_create(&res->thread,(const pthread_attr_t *)NULL/*attr*/,func,param);
}

int mpthread_join(mpthread_t thread){
  return pthread_join(thread.thread, NULL);
}

#elif (defined(WIN32))
/**********************************************************************************
 *   WIN32
 **********************************************************************************
 */
unsigned int system_cpu_number(){
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  if( sysinfo.dwNumberOfProcessors > MAX_CPU )return MAX_CPU;
  if (sysinfo.dwNumberOfProcessors == 0 )return 1;
  return sysinfo.dwNumberOfProcessors;
}

int mpthread_create(mpthread_t *res, void *(*func)(void *), void *param, mpthread_arrt_t *attr){
  res->thread = CreateThread((LPSECURITY_ATTRIBUTES)NULL/*attr*/, 0, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
  return 0;
}

int mpthread_join(mpthread_t thread){
  WaitForSingleObject(thread.thread, INFINITE);
  CloseHandle(thread.thread);
  return 0;
}

#endif

#ifdef MPPIPE_BYPASS
/**********************************************************************************
 **********************************************************************************
 *   ANONYMOUS PIPES - program implementation
 **********************************************************************************
 **********************************************************************************
 */
/*  Костыльная реализация анонимных каналов
  К сожалению, анонимные каналы в WinAPI не поддерживают асинхронных операций, а для именованных каналов
  приходится задавать имя в ////.//pipe/, кроме того, хотя мне удалось их запустить в неблокирующем режиме,
  последняя запись в переполненный канал завершается неудачей, причем эту неудачу невозможно отследить.
  Но при чтении этой сбойной записи считывается все что угодно кроме того что было записано, и ошибок не выдается
  Использование callback-функций, событий (event) и WriteFileEx / ReadFileEx ситуацию не меняет, более подробной
  информации нигде не нашел.
  Вероятно это настолько экзотика что никто ей не пользуется (интересно, а чем тогда пользуются?!) и не знает
  как заставить ее работать.
  Раз так, проще уж написать свою реализацию с нужным функционалом.
  Она использует более переносимые средства языка:
  - разделяемая между потоками (не процессами) память 
  - атомарные операции с челыми числами
  так что есть надежда на лучшую переносимость, как и опасения на счет производительности
 */

/* Описание принципа работы:
  "канал" представляет собой кольцевой буфер заданного размера с указателями головы (head) и хвоста (tail)
  Пи добавлении нового элемента он копируется (!) в голову, после чего она смещается и указывает на ПУСТУЮ
  ячейку, готовую к следующей записи. При выходе за пределы массива, голова перемещается в начало. Если бы
  работа шла с каждым элементом отдельно, можно было обрезать по маске BUF_MASK, но, поскольку работа идет
  с блоками, приходится проверять вручную. С хвостом - аналогично: он всегда указывает на ЗАНЯТУЮ счейку,
  готовую к чтению. Если голова и хвост указывают на одну и ту же ячейку - очередь пуста, если хвост на 1
  впереди головы - очередь заполнена и свободных мест нет. Примеры (00 - пустая ячейка, ## - заполненная,
  h - голова, t - хвост)
  
00 #t ## ## ## 0h 00 00 - 4 ячейка занято, 4 свободно
## ## 0h 00 00 #t ## ## - 5 занято, 4 свободно
00 ht 00 00 00 00 00 00 - все свободны (ht - голова и хвост указывают на одну и ту же ячейку)
## ## 0h #t ## ## ## ## - все заняты   
 */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUF_BITS
  #define BUF_BITS 14
#endif
#define BUF_SIZE (1<<BUF_BITS) /*размер буфера (байт) */
#define BUF_MASK (BUF_SIZE-1) /* маска, по которой будут обрезаться счетчика головы и хвоста */

#if (BUF_BITS <= 8)
  typedef uint8_t fifo_t;
#elif (BUF_BITS <= 16)
  typedef uint16_t fifo_t;
#elif (BUF_BITS <= 32)
  typedef uint32_t fifo_t;
#elif (BUF_BITS <= 64)
  typedef uint64_t fifo_t;
#else
  #error mppipe_buffer too large (are you REALLY need buffer size of 1e19 bytes ?! )
#endif

struct sProgramPipe{
  uint8_t buffer[ BUF_SIZE ];
  fifo_t tail, head;
};

char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd){
  struct sProgramPipe *pipe = malloc(sizeof(struct sProgramPipe));
  if(pipe == NULL)return MTP_ERR;
  pipe->tail = 0; pipe->head = 0;
  wr->fifo = rd->fifo = (void*)pipe;
  return MTP_OK;
}

unsigned long mppipe_read(mppipe_r rd, void *buf, size_t count){
  struct sProgramPipe *pipe = (struct sProgramPipe*)rd.fifo;
  fifo_t av_size=0, av_size2;
  if( pipe->head == pipe->tail )return 0; //буфер пуст - читать нечего, возвращаем 0
  
  if(pipe->tail > pipe->head){
    //голва оказалась сзади, значит доступные данные расположены как минимум от хвоста до конца буфера
    //возможно, еще есть кусок от начала буфера до головы, но об этом позже
    av_size = BUF_SIZE - pipe->tail; //сколько всего байт до конца буфера
    if(av_size >= count){
      //запрошено меньше чем доступно - выдаем сколько хотят и выйдем
      memcpy( buf, &pipe->buffer[pipe->tail], count);
      //это шаманство - атомарные операции с целыми числами. Оно замедляет работу, зато безопасно, быстрее и проще семафоров
      //по сути оно выполняет следующий код
      // tail += count;
      // if( tail == BUF_SIZE ) tail = 0
      if(__sync_add_and_fetch( &pipe->tail, count) == BUF_SIZE)__sync_and_and_fetch( &pipe->tail, 0);
      return count;
    }else{
      //запрошено больше чем було до конца буфера. Пока считаем сколько есть и пойдем проверять в начале
      memcpy( buf, &pipe->buffer[pipe->tail], av_size);
      if(__sync_add_and_fetch( &pipe->tail, av_size) == BUF_SIZE)__sync_and_and_fetch( &pipe->tail, 0);
    }
  }
  //доступные данные расположены от хвоста до головы (а не до конца буфера)
  av_size2 = pipe->head - pipe->tail;
  //если предыдущее условие отработало, считать надо на av_size байт меньше, чем запрошено
  buf += av_size;
  count -= av_size;
  //аналогично предыдущему - достаточно ли данных чтобы удовлетворить запрос полностью
  if(av_size2 >= count){
    memcpy( buf, &pipe->buffer[pipe->tail], count);
    __sync_add_and_fetch( &pipe->tail, count);
    return count+av_size;
  }else{
    memcpy( buf, &pipe->buffer[pipe->tail], av_size2);
    __sync_add_and_fetch( &pipe->tail, av_size2);
    return av_size2+av_size;
  }
}

unsigned long mppipe_write(mppipe_w wr, void *buf, size_t count){
  //тут аналогично чтению - особо комментировать не буду
  struct sProgramPipe *pipe = (struct sProgramPipe*)wr.fifo;
  fifo_t av_size=0, av_size2;
  if( pipe->tail - pipe->head == 1)return 0; //буфер переполнен
  
  if(pipe->head >= pipe->tail){
    av_size = BUF_SIZE - pipe->head;
    if(pipe->tail == 0)av_size--; //если хвост оказался в начале массива, трогать последний элемент нельзя!
    if(av_size >= count){
      memcpy( &pipe->buffer[pipe->head], buf, count );
      if(__sync_add_and_fetch( &pipe->head, count) == BUF_SIZE)__sync_and_and_fetch( &pipe->head, 0);
      return count;
    }else{
      memcpy( &pipe->buffer[pipe->head], buf, av_size );
      if(__sync_add_and_fetch( &pipe->head, av_size) == BUF_SIZE)__sync_and_and_fetch( &pipe->head, 0);
    }
    if(pipe->tail == 0)return av_size;
  }
  
  av_size2 = pipe->tail - pipe->head - 1;
  count -= av_size;
  buf += av_size;
  if(av_size2 >= count){
    memcpy( &pipe->buffer[pipe->head], buf, count );
    __sync_add_and_fetch( &pipe->head, count);
    return count+av_size;
  }else{
    memcpy( &pipe->buffer[pipe->head], buf, av_size2 );
    __sync_add_and_fetch( &pipe->head, av_size2);
    return av_size2+av_size;
  }
}


char mppipe_close_write(mppipe_w *wr){
  //освобождаем память только в дескрипторе записи. Особой разницы нет, главное не дублировать
  free(wr->fifo);
  return 0;
}
char mppipe_close_read(mppipe_r *rd){
  return 0;
}

#elif defined(linux)
/**********************************************************************************
 *   ANONYMOUS PIPES - Linux pipes
 **********************************************************************************
 */
//тут все достаточно очевидно, не то что в WinAPI-каналах или кустарной реализации
char mppipe_create_anon(mppipe_w *wr, mppipe_r *rd){
  int fd[2];
  if(pipe2(fd, O_NONBLOCK))return MTP_ERR; //это работает не везде!
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

#endif
