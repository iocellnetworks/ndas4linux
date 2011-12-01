
#ifndef __MIO_INIT_TASK_H__
#define __MIO_INIT_TASK_H__

class InitTask {
public:
    typedef void (*TaskHandler)(void *socket);
    InitTask(int socket, TaskHandler handler) : mySocket(socket), myHandler(handler) { };
    ~InitTask() { };

public:
    int mySocket;
    TaskHandler myHandler;
};

#endif // __MIO_INIT_TASK_H__
