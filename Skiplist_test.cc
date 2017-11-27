#include <string>
#include <cassert>
#include "stdio.h"
#include "Skiplist.hh"
#include "util.hh"

struct Rinbow
    : public RefCounted
    , public Linkable<Rinbow>
{
    Rinbow(std::string const &name, int when)
        : name(name)
        , when(when)
    {}
    std::string name;
    int         when;
};

using RinbowPtr = DerivedRefPtr<Rinbow>;

void test3() {
    Skiplist<Rinbow> skiplist;
    constexpr int total = 69819;

    for ( int i = 0; i < total; i++ ) {
        RinbowPtr ptr = makeRefPtr<Rinbow>("multiColor", i);
        skiplist.enqueue(ptr);
        assert(ptr != nullptr);
    }

    constexpr int nlist = 3;
    std::unique_ptr<Skiplist<Rinbow>> listptr[nlist];

    for ( int i = 0; i < nlist; i++ ) {
        listptr[i] = skiplist.dequeue_half();
        printf("ok %d...\n", i);
    }


    for ( int i = 0; i < nlist; i++ ) {
        RinbowPtr ptr;
        while ( (ptr = listptr[i]->dequeue()) != nullptr ) {
//            printf("when=%d\n", ptr->when);
        }
    }

    {
//        RinbowPtr ptr;
//        while ( (ptr = skiplist.dequeue()) != nullptr ) {
//            printf("when=%d\n", ptr->when);
//        }
    }

}

void test2() {
    Skiplist<Rinbow> skiplist;
    constexpr int total = 128;

    for ( int i = 0; i < total; i++ ) {
        RinbowPtr ptr = makeRefPtr<Rinbow>("multiColor", i);
        skiplist.enqueue(ptr);
        assert(ptr != nullptr);
    }

    std::unique_ptr<Skiplist<Rinbow>> listptr = skiplist.dequeue_half();

    for ( int i = 0; i < total / 2; i++ ) {
        RinbowPtr ptr = listptr->dequeue();
        printf("when:%d\n", ptr->when);
//        assert(ptr->when == i);

    }

    for ( int i = 0; i < total / 2; i++ ) {
        RinbowPtr ptr = skiplist.dequeue();
        printf("when:%d\n", ptr->when);
//        assert(ptr->when == total / 2 + i);

    }

}

void test() {
    Skiplist<Rinbow> skiplist;
    constexpr int total = 128;
    constexpr int firstPart = 55;
    constexpr int secondPart = total - firstPart;

    for ( int i = 0; i < total; i++ ) {
        RinbowPtr ptr = makeRefPtr<Rinbow>("multiColor", i);
        skiplist.enqueue(ptr);
        assert(ptr != nullptr);
    }

    for ( int i = 0; i < firstPart; i++ ) {
        RinbowPtr ptr = skiplist.dequeue();
        assert(ptr->when == i);
    }

    for ( int i = 0; i < firstPart; i++ ) {
        RinbowPtr ptr = makeRefPtr<Rinbow>("multiColor", i + 1000);
        skiplist.enqueue(ptr);
        assert(ptr != nullptr);
    }

    for ( int i = 0; i < secondPart; i++ ) {
        RinbowPtr ptr = skiplist.dequeue();
        assert(ptr->when == i + firstPart);
    }

    for ( int i = 0; i < firstPart; i++ ) {
        RinbowPtr ptr = skiplist.dequeue();
        assert(ptr->when == i + 1000);
    }


}

int main() {
//    for ( long i = 0; i < 100000000; ++i ) test();
//    test();
//    test2();
//    for ( long i = 0; i < 10000000; i++ ) test3();
    test3();
}
