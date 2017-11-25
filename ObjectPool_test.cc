#include "ObjectPool.hh"
#include "util.hh"

#include <stdio.h>
#include <memory>
#include <string>
#include <cassert>

struct Kitty {
    std::string name;
    int age;
    double weight;

    Kitty(std::string const &name = "<kitty>", int age = 4,
            double weight = 1.445)
        : name(name)
        , age(age)
    {}
    static void* operator new(std::size_t sz);
    static void operator delete(void *ptr);
};

ObjectPool<Kitty> kitty_pool(16, 2);

void*
Kitty::operator new(std::size_t sz)
{
    void *res = kitty_pool.alloc();
    assert(res);
    printf("allocated: %p\n", res);
    return res;
}

void
Kitty::operator delete(void *ptr)
{
    kitty_pool.reclaim(ptr);
    printf("deleted %p\n", ptr);
}

void test() {
    std::unique_ptr<Kitty> ptrs[150];
//    std::shared_ptr<Kitty> ptrs[150];
    for ( int i = 0; i < 150; i++ ) {
        ptrs[i] = std::make_unique<Kitty>("WowKitty", 12);
//        ptrs[i] = std::make_shared<Kitty>("Wolf", 943);
    }
}

int main() {
    test();
}
