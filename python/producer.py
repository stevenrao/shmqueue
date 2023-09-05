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