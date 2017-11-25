#include "util.hh"
#include <string>
#include <cassert>
#include <utility>
#include <thread>

#include <chrono>
#include <stdio.h>

struct FakeNews : public RefCounted {
    std::string title;
    int         content;
    FakeNews(std::string const &title, int content)
        : title(title)
        , content(content)
    {}
};

using FakeNewsPtr = DerivedRefPtr<FakeNews>;

void test() {
    std::string title = "CNN";
    int content = 12;

    FakeNewsPtr ptr = makeRefPtr<FakeNews>(title, content);
    FakeNewsPtr ptr2;

    for ( int i = 0; i < 10; i++ ) {
        std::thread tid(
                [i, ptr] () {
                    printf("Thread %d: title: %s\n", i, ptr->title.c_str());
                });
        tid.detach();
    }

    assert(ptr != nullptr);
    assert(ptr2 == nullptr);
    assert(ptr->title == title);
    assert(ptr->content == content);

    ptr2 = ptr;
    assert(ptr != nullptr);
    assert(ptr2 != nullptr);
    assert(ptr2->title == title);
    assert(ptr2->content == content);

    ptr2 = std::move(ptr);
    assert(ptr == nullptr);
    assert(ptr2 != nullptr);


}

int main() {
    for ( long i = 0; i < 1000000000; i++ ) test();
}
