#ifndef _CO_USER_HH_
#define _CO_USER_HH_

#include "Task.hh"
#include "TaskGroup.hh"
#include "GlobalMediator.hh"

#include <functional>
#include <memory>

class TaskHandle {
    template<class Fn, class... Args>
    friend TaskHandle
    go(Fn&& callback, Args&&... args);
    template<class Fn, class... Args>
    friend TaskHandle
    go_pure(Fn&& callback, Args&&... args);
    friend class TaskBundle;
private:
    TaskPtr ptr__;
};

class TaskBundle {
    template<class Fn, class... Args>
    friend TaskHandle
    go(Fn&& callback, Args&&... args);
    template<class Fn, class... Args>
    friend TaskHandle
    go_pure(Fn&& callback, Args&&... args);
private:
    TaskGroup group__;
public:
    TaskBundle &registe(TaskHandle &handle) {
        group__.registe(handle.ptr__);
        return *this;
    }
    TaskBundle &registe(TaskHandle &&handle) {
        return registe(handle);
    }
    void wait() {
        group__.wait();
    }
};

template<class Fn, class... Args>
TaskHandle
go(Fn&& callback, Args&&... args)
{
    TaskHandle taskHandle;
    taskHandle.ptr__ = std::make_shared<Task>(
            std::bind(std::forward<Fn>(callback), std::forward<Args>(args)...)); 
    globalMediator.addRunnable(taskHandle.ptr__);
    return taskHandle;
}

template<class Fn, class... Args>
TaskHandle
go_pure(Fn&& callback, Args&&... args)
{
    TaskHandle taskHandle;
    taskHandle.ptr__ = std::make_shared<Task>(
            std::bind(std::forward<Fn>(callback), std::forward<Args>(args)...)); 
    taskHandle.ptr__->isPure = true;
    globalMediator.addRunnable(taskHandle.ptr__);
    return taskHandle;
}

void
co_init()
{
    GlobalMediator::Init();
}

void
co_mainloop()
{
    globalMediator.run();
}

void
co_terminate()
{
    GlobalMediator::TerminateGracefully();
}


#endif /* _CO_USER_HH_ */

