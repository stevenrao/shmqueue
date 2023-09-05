CC = gcc
CFLAGS = -Wall -Werror -fPIC  -Wpedantic -Wall -O3 -g

# 设置库的名称
LIBRARY = libshmqueue.so

# 设置源文件列表
SOURCES = shmqueue.c

# 生成目标文件列表
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(LIBRARY)

# 生成共享库
$(LIBRARY): $(OBJECTS)
	$(CC) $(CFLAGS) -shared $(OBJECTS) -o $(LIBRARY)

# 编译源文件为目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY)