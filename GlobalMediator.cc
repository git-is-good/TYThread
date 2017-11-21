#include "GlobalMediator.hh"

void
GlobalMediator::Init()
{
    int threadNumber = determineThreads();
    Instance().pmgrs.resize(threadNumber);
    std::fill(Instance().begin(), Instance().end(),
            make_unqiue(PerThreadMgr));
}

void
GlobalMediator::addToRunnable(TaskPtr ptr)
{

}

void
GlobalMediator::run()
{

}

thread_local int GlobalMediator::thread_id_ = ++thread_id_counter;
std::atomic<int> GlobalMediator::thread_id_counter = {0};
