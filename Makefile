CC := clang++
INCLUDEPATH := -I/usr/local/include
LIBPATH := -L/usr/local/lib
CXXFLAGS := $(INCLUDEPATH) $(LIBPATH) --std=c++14 -g -O2

TARGETS := PerThreadMgr_test Task_test GlobalMediator_test user_test

user_test: Task.cc TaskGroup.cc PerThreadMgr.cc GlobalMediator.cc user_test.cc
	$(CC) $(CXXFLAGS) -o $@ $^ -lboost_context

GlobalMediator_test: Task.cc TaskGroup.cc PerThreadMgr.cc GlobalMediator.cc GlobalMediator_test.cc
	$(CC) $(CXXFLAGS) -D_UNIT_TEST_GLOBAL_MEDIATOR_ -o $@ $^ -lboost_context

PerThreadMgr_test: Task.cc TaskGroup.cc PerThreadMgr.cc PerThreadMgr_test.cc
	$(CC) $(CXXFLAGS) -D_UNIT_TEST_PER_THREAD_MGR_ -o $@ $^ -lboost_context

Task_test: Task.cc Task_test.cc TaskGroup.cc
	$(CC) $(CXXFLAGS) -D_UNIT_TEST_TASK_ -o $@ $^ -lboost_context

clean:
	rm -rf *~ *.dSYM *.o a.out $(TARGETS)
