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

void process_fakenews(FakeNewsPtr ptr) {
    printf("processing %s\n", ptr->title.c_str());
}

void process_fakenews_2(FakeNewsPtr &ptr) {
    printf("processing %s\n", ptr->title.c_str());

    /* ptr will be changed, since it's a ref */
    ptr = makeRefPtr<FakeNews>("FoxNews", 14);
}

void process_fakenews_3(FakeNewsPtr &&ptr) {
    FakeNewsPtr nptr = std::move(ptr);

    /* ptr will be empty */
    assert(nptr->title == "CNN");
}

void test() {
    std::string title = "CNN";
    int content = 12;

    FakeNewsPtr ptr = makeRefPtr<FakeNews>(title, content);
    FakeNewsPtr ptr2;

    for ( int i = 0; i < 10; i++ ) {
        std::thread tid(
                [i, ptr] () {
                    auto ptr_ = ptr;
                    printf("Thread %d: title: %s\n", i, ptr_->title.c_str());
                    if ( i % 3 == 0 ) {
                        process_fakenews(ptr_);
                        assert(ptr_ != nullptr && ptr_->title == "CNN");
                        ptr_ = nullptr;
                        assert(ptr_ == nullptr);
                    } else if ( i % 3 == 1 ) {
                        process_fakenews_2(ptr_);
                        assert(ptr_ != nullptr && ptr_->title == "FoxNews");
                    } else {
                        process_fakenews_3(std::move(ptr_));
                        assert(ptr_ == nullptr);
                    }
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
