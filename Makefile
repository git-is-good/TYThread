CC := clang++
CXXFLAGS := --std=c++14 -g

PerThreadMgr_test: Task.cc TaskGroup.cc PerThreadMgr.cc PerThreadMgr_test.cc
	$(CC) $(CXXFLAGS) -D_UNIT_TEST_PER_THREAD_MGR_ -o $@ $^ -lboost_context

Task_test: Task.cc Task_test.cc TaskGroup.cc
	$(CC) $(CXXFLAGS) -D_UNIT_TEST_TASK_ -o $@ $^ -lboost_context

clean:
	rm -rf *~ *.dSYM *.o Task_test
