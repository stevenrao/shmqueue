#include "shmqueue.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/syscall.h>

#define CREATE_MODE  1
#define ATTACH_MODE  2

#define QUEUE_MAGIC_1 0x12
#define QUEUE_MAGIC_2 0x34
#define QUEUE_VERSION 1

pid_t gettid(void);

// 内置函数
static inline uint32_t _get_next_power_of_two(const uint32_t n)
{
	uint32_t x = 1;
	while (x < n) x <<= 1;
	return x;
}

static int32_t _create(shmqueue_t* q, const char* pathname, const uint32_t size, int32_t create_mode  )
{
	int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	int fd = open( pathname, O_RDWR | O_CREAT, permissions );
	if (fd == -1) 
    {
		perror("[shmqueue] _create open");
		return -1;
	}

	int protection = PROT_READ | PROT_WRITE;
	int visibility = MAP_SHARED;
	uint32_t rounded_size = _get_next_power_of_two(size);

	uint32_t mmap_size = sizeof(queue_t) + rounded_size ;

    printf("[shmqueue] _create mmap_size:%d:%d\n", rounded_size, mmap_size);

    if ( create_mode == CREATE_MODE )
    {
        if ( ftruncate(fd, mmap_size + 16 ) == -1) 
        {
            perror("[shmqueue] _create ftruncate");
            close(fd);
            return -1;
        }
        printf("[shmqueue] _create ftruncate success\n");

        if ( flock( fd, LOCK_EX ) == -1 ) 	
        {
            perror("[shmqueue] _create flock");
            close(fd);
            return -1;
        }
        printf("[shmqueue] _create flock success\n");
    }

	void* addr = mmap( NULL, mmap_size + 16, protection, visibility, fd, 0 );
	if ( addr == MAP_FAILED )	
    {
		perror("[shmqueue] _create  mmap");
		close(fd);
		return -1;
	}

	queue_t* qh = (queue_t*) addr;
	uint8_t* magic = qh->_magic;

    if ( create_mode == CREATE_MODE )
    {
        magic[0] = QUEUE_MAGIC_1;
        magic[1] = QUEUE_MAGIC_2;
        qh->_rpos = 0;
        qh->_wpos = 0;
        qh->_version = QUEUE_VERSION;
        qh->_size = rounded_size;
    }
    else
    {
        if ( ( magic[0] != QUEUE_MAGIC_1 || magic[1] != QUEUE_MAGIC_2 )
           || ( qh->_size != rounded_size ) )
           {
                //共享内存数据错误
                perror("[shmqueue] _create create: Existing buffer does not match specified size");
                munmap( addr, mmap_size );
                flock( fd, LOCK_UN );
                close( fd );
                return -1;
           }
    }

	//flock(fd, LOCK_UN);
	close(fd);
	q->_queue = qh;
    q->_size = mmap_size + 16;

	return 0;
}

static inline uint32_t _read(queue_t* data, uint32_t offset, char* dest, const uint32_t n)
{
	memcpy(dest, data->_buf + offset, n);
    // printf("[shmqueue] _read ==> offset:%d, n:%d\n", offset, n);
	return n;
}

static inline uint32_t _write(queue_t* data, uint32_t offset, const char* src, const uint32_t n)
{
	memcpy(data->_buf + offset, src, n);
    // printf("[shmqueue] _write ==> offset:%d, n:%d\n", offset, n);
	return n;
}

// 外部接口函数
int32_t create( shmqueue_t* q, const char* pathname, const uint32_t size, int32_t mode ) 
{
    int32_t create_mode = CREATE_MODE;
    if ( mode == CONSUMER_MODE )  create_mode = ATTACH_MODE;

	if ( _create( q, pathname, size, create_mode ) == -1)
    {
		return -1;
	}

    q->_mode = mode;

	return 0;
}

int32_t put( shmqueue_t* q, const void* src, const uint32_t n) 
{
	if ( q->_mode != PRODUCER_MODE || n == 0  ) return 0;

    while( 1 ) 
    {
        queue_t* data = (queue_t*)q->_queue;
        uint32_t wpos, new_wpos;
        __atomic_load(&data->_wpos, &wpos, __ATOMIC_ACQUIRE);

        uint32_t rpos;
        __atomic_load(&data->_rpos, &rpos, __ATOMIC_ACQUIRE);

        uint32_t size = data->_size;

        // 所有数据保持4字节对齐
        assert( wpos%4 == 0  && rpos%4 == 0  &&  size%4 == 0);

        new_wpos = (wpos + size) % size;
        rpos = (rpos + size) % size;

        // 如果剩余空间不足以存放数据, 不能全部写满，保留4个字节隔开 rpos 和 wpos
        if (  ( size - ( size  - rpos + new_wpos )% size) <  ( sizeof(uint32_t) + n + 4) )
        {
            // printf("[shmqueue] put ==> no space left:%d, to write:%d, rpos:%d, new_wpos:%d , n:%d )\n", (size - (size  - rpos + new_wpos )% size), data_offset + n, rpos, new_wpos, n);
            return 0;
        }

        uint32_t msg_size = n;
        new_wpos += _write(data, new_wpos % size, (char*) (&msg_size), sizeof(msg_size));
        new_wpos = (new_wpos + size) % size;

        // 如果 wpos + data_offset + n 超过 size, 则需要分两次写入
        if ( new_wpos + n > size )
        {
            // printf("[shmqueue] put 2 step 0==> new_wpos:%d, n:%d, size:%d\n", new_wpos, n, size);
            uint32_t first_write = size - new_wpos;
            uint32_t second_write = n - first_write;
            new_wpos += _write(data, new_wpos % size, (char*) src, first_write);
            new_wpos = (new_wpos + size) % size;

            src = (char*) src + first_write;
            new_wpos += _write(data, new_wpos % size, (char*) src, second_write);
            new_wpos = (new_wpos + size) % size;
            // printf("[shmqueue] put 2 step 2==> new_wpos:%d, n:%d, size:%d\n", new_wpos, n, size);
        }
        else
        {
            new_wpos += _write(data, new_wpos % size, (char*) src, n);
            new_wpos = (new_wpos + size) % size;
            // printf("[shmqueue] put 1 step ==> new_wpos:%d, n:%d, size:%d\n", new_wpos, n, size);
        }

        // wpos 4字节对齐
        new_wpos = ( new_wpos + 3 ) & ~3;
        //CAS 赋值
        if ( __atomic_compare_exchange_n( &data->_wpos, &wpos, new_wpos, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE ) )
        {
            // printf("[shmqueue] put ==> __atomic_compare_exchange_n success wpos:%d, new_wpos:%d\n", wpos, new_wpos);
            break;
        }
        else
        {
            // printf("[shmqueue:%d] put ==> __atomic_compare_exchange_n failed wpos:%d, new_wpos:%d\n", (int32_t)gettid(), wpos, new_wpos);
        }
    }
    // printf("[shmqueue] put ==> msg_size:%d\n", n);
	return n;
}

int32_t get( shmqueue_t* q, void* dst, const uint32_t n) 
{
    while( 1 )
    {
        queue_t* data = (queue_t*)q->_queue;
        uint32_t rpos, new_rpos, rtime;
        uint32_t wpos;

        __atomic_load(&data->_rpos, &rpos, __ATOMIC_ACQUIRE);
        __atomic_load(&data->_wpos, &wpos, __ATOMIC_ACQUIRE);

        uint32_t size = data->_size;
        // 所有数据保持4字节对齐
        assert( wpos%4 == 0  && rpos%4 == 0  && size%4 == 0);
        new_rpos = (rpos + size) % size;

        //队列为空
        if ( rpos == wpos  || new_rpos == wpos)
        {
            // printf("[shmqueue] get empty ==> rpos == wpos (%d)\n", wpos);
            return 0;
        }
        wpos = (wpos + size) % size;
        
        uint32_t msg_size;
        new_rpos += _read(data, new_rpos % size, (char *)(&msg_size), sizeof(msg_size));
        new_rpos = (new_rpos + size) % size;

        // 是否要分两次读出
        if ( new_rpos + msg_size > size )
        {
            // printf("[shmqueue] get 2 step 0==> new_rpos:%d, msg_size:%d, size:%d\n", new_rpos, msg_size, size);
            uint32_t first_read = size - new_rpos;
            uint32_t second_read = msg_size - first_read;
            new_rpos += _read(data, new_rpos % size, (char*) dst, first_read);
            new_rpos = (new_rpos + size) % size;

            dst = (char*) dst + first_read;
            new_rpos += _read(data, new_rpos % size, (char*) dst, second_read);
            new_rpos = (new_rpos + size) % size;
            // printf("[shmqueue] get 2 step 2==> new_rpos:%d, msg_size:%d, size:%d\n", new_rpos, msg_size, size);
        }
        else
        {
            new_rpos += _read(data, new_rpos % size, (char*) dst, msg_size);
            new_rpos = (new_rpos + size) % size;
            // printf("[shmqueue] get 1 step ==> msg_size:%d:%d\n", msg_size, new_rpos);
        }

        // new_rpos 4字节对齐
        new_rpos = (new_rpos + 3) & ~3;

        if ( __atomic_compare_exchange_n( &data->_rpos, &rpos, new_rpos, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE ) )
        {
            // printf("[shmqueue] get ==> __atomic_compare_exchange_n sucess rpos:%d, new_rpos:%d, msg_size:%d, n:%d\n", rpos, new_rpos, msg_size, n);
            return msg_size;
        }
        else
        {
            // printf("[shmqueue] get ==> __atomic_compare_exchange_n failed rpos:%d, new_rpos:%d, msg_size:%d, n:%d\n", rpos, new_rpos, msg_size, n);
        }   
    }

    return 0;
}

// 写入队列的数据大小
int32_t size( const shmqueue_t* q) 
{
	uint32_t wpos;
	uint32_t rpos;
	__atomic_load(&(q->_queue->_wpos), &wpos, __ATOMIC_ACQUIRE);
	__atomic_load(&(q->_queue->_rpos), &rpos, __ATOMIC_ACQUIRE);

    return ( q->_queue->_size - rpos + wpos )% q->_queue->_size;
}

// 队列的容量
int32_t capacity(const shmqueue_t* q) 
{
    return q->_queue->_size - 4;
}

// 队列剩余空间
int32_t left( const shmqueue_t* q) 
{
    uint32_t wpos;
    uint32_t rpos;
    __atomic_load(&(q->_queue->_wpos), &wpos, __ATOMIC_ACQUIRE);
    __atomic_load(&(q->_queue->_rpos), &rpos, __ATOMIC_ACQUIRE);

    return q->_queue->_size - 4 - ( q->_queue->_size - ( wpos - rpos ) )% q->_queue->_size;
}

// wpos == rpos 表示队列为空, 
int32_t empty( const shmqueue_t* q) 
{
	uint32_t wpos;
	uint32_t rpos;
	__atomic_load(&(q->_queue->_wpos), &wpos, __ATOMIC_ACQUIRE);
	__atomic_load(&(q->_queue->_rpos), &rpos, __ATOMIC_ACQUIRE);

    return wpos == rpos;
}

//(wpos + 4) % size == rpos 表示队列为满
int32_t full( const shmqueue_t* q) 
{
    uint32_t wpos;
    uint32_t rpos;
    __atomic_load(&(q->_queue->_wpos), &wpos, __ATOMIC_ACQUIRE);
    __atomic_load(&(q->_queue->_rpos), &rpos, __ATOMIC_ACQUIRE);

    return ( (wpos + 4) % q->_queue->_size )== rpos;
}

// 销毁队列
void destroy( shmqueue_t* q) 
{
    uint32_t map_size = q->_size;
	munmap( (void *)( q->_queue ) , map_size );
}
