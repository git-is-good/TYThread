#include "GlobalScheduler.hh"

void
GlobalScheduler::Init()
{
    int threadNumber = determineThreads();
    Instance().pmgrs.resize(threadNumber);
    std::fill(Instance().begin(), Instance().end(),
            make_unqiue(PerThreadMgr));
}

void
GlobalScheduler::addToRunnableQueue(TaskPtr ptr)
{

}

void
GlobalScheduler::run()
{

}

thread_local int GlobalScheduler::thread_id_ = ++thread_id_counter;
std::atomic<int> GlobalScheduler::thread_id_counter = {0};
