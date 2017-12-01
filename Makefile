CC := clang++
INCLUDEPATH := -I/usr/local/include
LIBPATH := -L/usr/local/lib
CXXFLAGS := --std=c++14 -g -O2
AR := ar

OMPCC := /usr/local/opt/llvm/bin/clang
OMPLIBPATH := -L/usr/local/opt/llvm/lib
OMPCXXFLAGS := -fopenmp -O2

HEADERS :=					\
	Config.hh				\
	GlobalMediator.hh		\
	ObjectPool.hh			\
	PerThreadMgr.hh			\
	Skiplist.hh				\
	Spinlock.hh				\
	Task.hh					\
	TaskGroup.hh			\
	co_user.hh				\
	debug.hh				\
	debug_local_begin.hh	\
	debug_local_end.hh		\
	util.hh				

LIBS := -lboost_context

YAMITHREAD_LIB_OBJS :=		\
	GlobalMediator.o		\
	PerThreadMgr.o			\
	Task.o					\
	TaskGroup.o				\

OBJS := $(YAMITHREAD_LIB_OBJS) user_test.o GlobalMediator_test.o

GENLIBS := libyami_thread.a

EXECS := user_test GlobalMediator_test

TARGETS := $(GENLIBS) $(EXECS)

all: $(TARGETS)

libyami_thread.a: $(YAMITHREAD_LIB_OBJS)
	$(AR) rcs $@ $(YAMITHREAD_LIB_OBJS)

user_test: user_test.o $(GENLIBS)
	$(CC) $(CXXFLAGS) $(LIBPATH) -o $@ $^ $(LIBS)

GlobalMediator_test: GlobalMediator_test.o $(GENLIBS)
	$(CC) $(CXXFLAGS) $(LIBPATH) -o $@ $^ $(LIBS)

omp_test: omp_test.c
	$(OMPCC) $(OMPCXXFLAGS) $(OMPLIBPATH) -o $@ $^

omp_merge_sort: omp_merge_sort.c
	$(OMPCC) $(OMPCXXFLAGS) $(OMPLIBPATH) -o $@ $^

%.o: %.cc $(HEADERS)
	$(CC) -c $(CXXFLAGS) $(INCLUDEPATH) -o $@ $<

clean:
	rm -rf *~ *.dSYM a.out omp_test

clean-real:
	rm -rf *~ *.dSYM a.out omp_test $(TARGETS) $(GENLIBS) $(OBJS)
