#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <stdint.h>


typedef struct{
  uint8_t* p_o;        /**< Original pointer */
  uint8_t* volatile p_r;   /**< Read pointer */
  uint8_t* volatile p_w;   /**< Write pointer */
  volatile int32_t fill_cnt;  /**< Number of filled slots */
  int32_t size;       /**< Buffer size */
  int32_t block_size;
}RINGBUF;

int32_t rb_init(RINGBUF *r, uint8_t* buf, int32_t size, int32_t block_size);
int32_t rb_put(RINGBUF *r, uint8_t* c);
int32_t rb_get(RINGBUF *r, uint8_t* c);
int32_t rb_available(RINGBUF *r);
uint32_t rb_read(RINGBUF *r, uint8_t *buf, int len);
uint32_t rb_write(RINGBUF *r, uint8_t *buf, int len);

#endif
