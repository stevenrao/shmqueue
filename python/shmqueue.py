import ctypes
import os

class ShmQueue:
    def __init__(self, pathname, size, mode):
        """
        创建共享内存队列的实例。

        :param pathname: 共享内存队列的路径名。
        :param size: 队列的大小。
        :param mode: 权限模式。
        """
        self.libshmqueue = ctypes.cdll.LoadLibrary(os.path.abspath("libshmqueue.so"))
        
        class QueueStruct(ctypes.Structure):
            _fields_ = [
                ("_magic", ctypes.c_uint8 * 2),
                ("_version", ctypes.c_int32),
                ("_size", ctypes.c_uint32),
                ("__pad0", ctypes.c_char * 64),
                ("_rpos", ctypes.c_uint32),
                ("__pad1", ctypes.c_char * 64),
                ("_wpos", ctypes.c_uint32),
                ("__pad2", ctypes.c_char * 64),
                ("_buf", ctypes.c_char)
            ]
        
        class ShmQueueStruct(ctypes.Structure):
            _fields_ = [
                ("_queue", ctypes.POINTER(QueueStruct)),
                ("_mode", ctypes.c_int32),
                ("_size", ctypes.c_int32)
            ]
        
        self.libshmqueue.create.restype = ctypes.c_int32
        self.libshmqueue.create.argtypes = [ctypes.POINTER(ShmQueueStruct), ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int32]
        
        self.libshmqueue.put.restype = ctypes.c_int32
        self.libshmqueue.put.argtypes = [ctypes.POINTER(ShmQueueStruct), ctypes.c_void_p, ctypes.c_uint32]
        
        self.libshmqueue.get.restype = ctypes.c_int32
        self.libshmqueue.get.argtypes = [ctypes.POINTER(ShmQueueStruct), ctypes.c_void_p, ctypes.c_uint32]
        
        self.libshmqueue.size.restype = ctypes.c_int32
        self.libshmqueue.size.argtypes = [ctypes.POINTER(ShmQueueStruct)]
        
        self.libshmqueue.left.restype = ctypes.c_int32
        self.libshmqueue.left.argtypes = [ctypes.POINTER(ShmQueueStruct)]

        self.libshmqueue.capacity.restype = ctypes.c_int32
        self.libshmqueue.capacity.argtypes = [ctypes.POINTER(ShmQueueStruct)]
        
        self.libshmqueue.empty.restype = ctypes.c_int32
        self.libshmqueue.empty.argtypes = [ctypes.POINTER(ShmQueueStruct)]
        
        self.libshmqueue.full.restype = ctypes.c_int32
        self.libshmqueue.full.argtypes = [ctypes.POINTER(ShmQueueStruct)]

        self.libshmqueue.destroy.argtypes = [ctypes.POINTER(ShmQueueStruct)]
        
        self.q = ShmQueueStruct()
        result = self.libshmqueue.create(ctypes.byref(self.q), pathname.encode(), size, mode)
        if result != 0:  
            raise Exception("Failed to create ShmQueue")

    def put(self, data):
        """
        将数据放入共享内存队列。

        :param data: 要放入队列的数据。
        :return: 成功放入数据时返回实际放入的字节数，失败时返回负数。
        """
        data_ptr = ctypes.c_char_p(data)
        length = len(data)
        result = self.libshmqueue.put(ctypes.byref(self.q), data_ptr, length)
        return result
    
    def get(self, length):
        """
        从共享内存队列中获取数据。

        :param length: 要获取的数据长度。
        :return: 成功获取数据时返回获取的二进制数据，失败时返回空的字节串。
        """
        buffer = (ctypes.c_char * ( length + 16))()
        result = self.libshmqueue.get(ctypes.byref(self.q), buffer, length)
        binary_data = bytes(buffer)[:result]
        return binary_data
    
    def get_size(self):
        """
        获取队列当前的大小。

        :return: 当前队列的大小。
        """
        return self.libshmqueue.size(ctypes.byref(self.q))

    def get_left(self):
        """
        获取队列的剩余空间大小。

        :return: 队列的剩余空间大小。
        """
        return self.libshmqueue.left(ctypes.byref(self.q))
        
    def get_capacity(self):
        """
        获取队列的总容量大小。

        :return: 队列的总容量大小。
        """
        return self.libshmqueue.capacity(ctypes.byref(self.q))
    
    def is_empty(self):
        """
        检查队列是否为空。

        :return: 如果队列为空，则返回 True，否则返回 False。
        """
        return bool(self.libshmqueue.empty(ctypes.byref(self.q)))
    
    def is_full(self):
        """
        检查队列是否已满。

        :return: 如果队列已满，则返回 True，否则返回 False。
        """
        return bool(self.libshmqueue.full(ctypes.byref(self.q)))
        
    def destroy(self):
        """
        销毁共享内存队列。
        """
        self.libshmqueue.destroy(ctypes.byref(self.q))