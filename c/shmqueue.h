/**
 * @file shmqueue.h
 * @brief 环形队列，用于线程或进程间通信。适用于单生产者单消费者（SPSC）或单生产者多消费者（SPMC）场景，不适用于多生产者场景。
 * @date 2023/09/04
 * @email raochaoxun1@huya.com
 */

#ifndef _SHM_QUEUE_H_
#define _SHM_QUEUE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PRODUCER_MODE  1
#define CONSUMER_MODE  2

// 共享内存中
typedef struct queue_t
{
	uint8_t     _magic[2];
	int32_t     _version;
	uint32_t    _size;
	char        __pad0[64];

	uint32_t    _rpos;
    uint32_t    _rtime;
	char        __pad1[64];

	uint32_t    _wpos;
    uint32_t    _wtime;
	char        __pad2[64];

	char        _buf[];
} queue_t;

// 进程内存中
typedef struct shmqueue_t
{
    queue_t * _queue;
    int32_t   _mode;
    int32_t   _size;
} shmqueue_t;


/**
 * 创建一个共享内存队列。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，用于保存创建的队列信息。
 * @param pathname - 共享内存队列的路径名。
 * @param size - 队列的大小。
 * @param mode - 权限模式。
 * @return 成功创建队列时返回 0，否则返回负数。
 */
int32_t create(shmqueue_t* q, const char* pathname, const uint32_t size, int32_t mode);

/**
 * 将数据放入共享内存队列。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要放入数据的队列。
 * @param src - 指向数据源的指针，即要放入队列的数据。
 * @param n - 要放入的数据的大小。
 * @return 成功放入数据时返回实际放入的字节数，否则返回负数。
 */
int32_t put(shmqueue_t* q, const void* src, const uint32_t n);

/**
 * 从共享内存队列中获取数据。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要获取数据的队列。
 * @param dst - 指向目标缓冲区的指针，即接收获取的数据的缓冲区。
 * @param n - 要获取的数据的大小。
 * @return 成功获取数据时返回实际获取的字节数，否则返回负数。
 */
int32_t get(shmqueue_t* q, void* dst, const uint32_t n);

/**
 * 获取队列的当前大小。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要查询的队列。
 * @return 当前队列的大小。
 */
int32_t size(const shmqueue_t* q);

/**
 * 获取队列的剩余空间大小。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要查询的队列。
 * @return 当前队列的剩余空间大小。
 */
int32_t left(const shmqueue_t* q);

/**
 * 获取队列的总容量大小。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要查询的队列。
 * @return 当前队列的总容量大小。
 */
int32_t capacity(const shmqueue_t* q);

/**
 * 检查队列是否为空。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要检查的队列。
 * @return 如果队列为空则返回非零值，否则返回 0。
 */
int32_t empty(const shmqueue_t* q);

/**
 * 检查队列是否已满。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要检查的队列。
 * @return 如果队列已满则返回非零值，否则返回 0。
 */
int32_t full(const shmqueue_t* q);

/**
 * 销毁共享内存队列。
 * 
 * @param q - 指向 shmqueue_t 结构体的指针，表示要销毁的队列。
 */
void destroy(shmqueue_t* q);

#endif
