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