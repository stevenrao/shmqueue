# shmqueue
### 1、介绍
这是一个多进程/线程 无锁共享内存环形队列。 使用c 代码编写。 
同时提供了一份python调用代码

### 2、原理
####### 1、利用mmap 开辟一块共享内存区域，在这块连续内存上创建一个环形buffer。 形成一个环形队列；
####### 2、数据和游标是在一起的，(不像其他某些实现，分成了数据和node 两个部分。)
####### 3、另外数据是支持变长的，不要求固定长度
####### 4、不用加锁

### 3、依赖
本项目只实现了linux下的。
python实现 在python3 环境下

### 4、限制
只适合 spsc （单生产者-单消费者）或 spmc（单生产者-多消费者）
不能用在 mpmc 场景下

### 5、使用
生产者
```
import random
import string
import time
from multiprocessing import Process
from shmqueue import ShmQueue

def generate_random_string(length):
    characters = string.ascii_letters + string.digits
    random_string = ''.join(random.choices(characters, k=length))
    return random_string

def producer():
    # 创建 ShmQueue 实例
    pathname = "/home/stevenrao/work/queue/shmqueue.map"
    size = 1024
    mode = 1  # PRODUCER_MODE
    q = ShmQueue(pathname, size, mode)

    while True:
        s = "[:]" + generate_random_string(random.randint(10, 100))
        q.put(s.encode('utf-8'))

if __name__ == '__main__':
    producer()        
```

消费者
```
import random
import string
import time
from multiprocessing import Process
from shmqueue import ShmQueue

def generate_random_string(length):
    characters = string.ascii_letters + string.digits
    random_string = ''.join(random.choices(characters, k=length))
    return random_string

def consumer():
    # 创建 ShmQueue 实例
    pathname = "/home/stevenrao/work/queue/shmqueue.map"
    size = 1024
    mode = 2  # CONSUMER_MODE
    q = ShmQueue(pathname, size, mode)

    while True:
        data = q.get(200)
        string_data = data.decode('utf-8')
        if len(string_data) > 0:  print(string_data)

if __name__ == '__main__':
    consumer()              
```


