#ifndef _SKIPLIST_HH_
#define _SKIPLIST_HH_

#include "debug.hh"
#include "util.hh"
#include "Spinlock.hh"

#include <array>
#include <memory>
#include <utility>
#include <type_traits>

#define DEBUG_SKIPLIST__

#ifdef DEBUG_SKIPLIST__

#include <stdio.h>
#define DEBUG_PRINT_LOCAL(fmt, ...)             \
        fprintf(stderr, "%s:%d:%s: " fmt "\n",  \
                __FILE__, __LINE__, __func__,   \
                ##__VA_ARGS__);                 \

#else

#define DEBUG_PRINT_LOCAL(fmt, ...)

#endif /* DEBUG_SKIPLIST__ */

/* for intrusive linked list */
template<class Derived, bool UseRefPtr = true>
class Linkable : public NonCopyable {
public:
    ~Linkable() = default;
protected:
    Linkable()
        : next(nullptr)
    {}

private:
    template<class T, int SkipGap>
    friend class Skiplist;
    Derived     *next;
};

template<class T, int SkipGap = 4>
class Skiplist : public NonCopyable {
public:
    using ref_ptr_type = DerivedRefPtr<T>;

    void enqueue(DerivedRefPtr<T> const &ptr) {
        if ( head ) {
            tail->next = const_cast<T*>(ptr.get());
            tail = tail->next;
        } else {
            head = tail = const_cast<T*>(ptr.get());
        }
        tail->next = nullptr;
        ref_ptr_type::increase(tail);
        _update_after_enque();
    }

    void enqueue(DerivedRefPtr<T> &&ptr) {
        if ( head ) {
            tail->next = ptr.get();
            tail = tail->next;
        } else {
            head = tail = ptr.get();
        }
        ptr.set(nullptr);
        _update_after_enque();
    }

    DerivedRefPtr<T> dequeue() {
        if ( !head ) {
            return std::move(DerivedRefPtr<T>{});
        }
        T *t = head;
        head = head->next;

        DerivedRefPtr<T> res;
        res.set(t);

        if ( t == candidate ) {
            candidate = nullptr;
        }
        if ( ++head_off % SkipGap == 0 && layers[0].headNode &&
                (void*)layers[0].headNode->downNode == t ) {
            DEBUG_PRINT_LOCAL("head node in layers[0] will be deleted:"
                    "head_off:%d,tail_off:%d", head_off, tail_off);

            // loop invariant: delete head node at this layer
            for ( int i = 0; i < layers.size(); ++i ) {
                Layer *thisLayer = &layers[i];

                if ( thisLayer->headNode.get() == thisLayer->candidate ) {
                    thisLayer->candidate = nullptr;
                }

                SkipNode *oldHead = thisLayer->headNode.get();
                thisLayer->headNode = std::move(thisLayer->headNode->nextNode);
                if ( ++thisLayer->head_off % SkipGap == 0 &&
                        i + 1 < layers.size() && layers[i + 1].headNode
                       && layers[i + 1].headNode->downNode == oldHead ) {
                    DEBUG_PRINT_LOCAL("head node in layers[%d] will be deleted:"
                            "head_off:%d,tail_off:%d", i + 1, thisLayer->head_off, thisLayer->tail_off);
                    continue;
                } else {
                    break;
                }
            }
        }
        return std::move(res);
    }

    std::unique_ptr<Skiplist<T>> dequeue_half() {
        int layerToSplit = layers.size();
        while ( --layerToSplit >= 0 ) {
            int sz = layers[layerToSplit].tail_off - layers[layerToSplit].head_off;
            if ( sz > 1 ) {
                /* this layer has at least 2 nodes, split it */
                break;
            } else if ( sz == 1 ) {
                /* this non-empty layer is useless */
                DEBUG_PRINT_LOCAL("a non-empty layer %d cleanuped", layerToSplit);
                layers[layerToSplit].cleanup();
            }
        }

        if ( layerToSplit >= 0 ) {
            /* give away the first (s + 1) / 2 to other,
             * because the last one is always full;
             * and give away the first part doesn't need to
             * modify the upper layers
             */

            DEBUG_PRINT_LOCAL("layerToSplit found: %d", layerToSplit);

            Layer *thisLayer = &layers[layerToSplit];
            int toGiveAway = (thisLayer->tail_off - thisLayer->head_off + 1) / 2;

            SkipNode *lastToGiveAway = thisLayer->headNode.get();
            for ( int i = 0; i < toGiveAway - 1; i++ ) {
                lastToGiveAway = lastToGiveAway->nextNode.get();
            }
            SkipNode *cur = lastToGiveAway;

            std::unique_ptr<Skiplist> res = std::make_unique<Skiplist>();
            int toGiveAwayCurLayer = toGiveAway;
            for ( int i = layerToSplit; i >= 0; --i ) {
                std::unique_ptr<SkipNode> cur_k = std::move(cur->nextNode);
                MUST_TRUE(cur_k != nullptr, "");

                /* how many next layer nodes to give away */
                int toGiveAwayNextLayer = toGiveAwayCurLayer * SkipGap - layers[i].head_off % SkipGap;

                res->layers[i].headNode = std::move(layers[i].headNode);
                layers[i].headNode = std::move(cur_k);

                res->layers[i].tailNode = cur;
                /* layers[i].tailNode no change */

                res->layers[i].head_off = layers[i].head_off;
                res->layers[i].tail_off = layers[i].head_off + toGiveAwayCurLayer;
                layers[i].head_off = layers[i].tail_off - (layers[i].tail_off - layers[i].head_off - toGiveAwayCurLayer);
                /* layers[i].tail_off no change */

//                cur = static_cast<SkipNode*>(cur->downNode);
                cur = static_cast<SkipNode*>(cur->lastDownNode);
                toGiveAwayCurLayer = toGiveAwayNextLayer;
            }

            /* process final raw data */
            res->head = head;
            head = reinterpret_cast<T*>(cur)->next;

//            DEBUG_PRINT_LOCAL("!!!!!! when=%d,when=%d", res->head->when, head->when);

            res->tail = reinterpret_cast<T*>(cur);
            res->tail->next = nullptr;

            res->head_off = head_off;
            res->tail_off = head_off + toGiveAwayCurLayer;

            head_off = tail_off - (tail_off - head_off - toGiveAwayCurLayer);
            return std::move(res);
        } else {
            /* split the raw data */
            std::unique_ptr<Skiplist> res;

            int toGiveAway = (tail_off - head_off + 1) / 2;
            if ( toGiveAway == 0 ) {
                DEBUG_PRINT_LOCAL("Nothing to give away");
                return std::move(res);
            }

            DEBUG_PRINT_LOCAL("Give away raw data: toGiveAway:%d", toGiveAway);
            T* lastToGiveAway = head;
            for ( int i = 0; i < toGiveAway - 1; i++ ) {
                lastToGiveAway = lastToGiveAway->next;
            }

            res->head = head;
            head = lastToGiveAway->next;

            res->tail = lastToGiveAway;
            res->head_off = head_off;
            res->tail_off = head_off + toGiveAway;
            head_off = tail_off - (tail_off - head_off - toGiveAway);
            return std::move(res);
        }
    }

    ~Skiplist() {
        if ( head ) {
            MUST_TRUE(tail_off > head_off, "");
            DEBUG_PRINT_LOCAL("Destructor of Skiplist called when head_off:%d,tail_off:%d",
                   head_off, tail_off);
            while ( head != tail ) {
                ref_ptr_type::decrease(head);
                head = head->next;
            }
            ref_ptr_type::decrease(head);
        }
    }

private:
    void _update_after_enque() {
        ++tail_off;
        if ( candidate && tail_off % SkipGap == 0 ) {
            MUST_TRUE(tail_off - head_off >= SkipGap, "internal error");
            void *cur = candidate;
            void *last = tail;

//            DEBUG_PRINT_LOCAL("!!!%d", candidate->when);
            candidate = nullptr;

            DEBUG_PRINT_LOCAL("adding node to layers[0]:"
                    "head_off:%d,tail_off:%d", head_off, tail_off);

            // loop invariant: a new node is going to be added at this layer 
            for ( int i = 0; i < layers.size(); ++i ) {
                Layer *thisLayer = &layers[i];
                if ( !thisLayer->headNode ) {
                    thisLayer->headNode = std::make_unique<SkipNode>();
                    thisLayer->tailNode = thisLayer->headNode.get();
                } else {
                    thisLayer->tailNode->nextNode = std::make_unique<SkipNode>();
                    thisLayer->tailNode = thisLayer->tailNode->nextNode.get();
                }
                thisLayer->tailNode->downNode = cur;
                thisLayer->tailNode->lastDownNode = last;
                ++thisLayer->tail_off;
                if ( thisLayer->candidate && thisLayer->tail_off % SkipGap == 0 ) {
                    MUST_TRUE(thisLayer->tail_off - thisLayer->head_off >= SkipGap, "internal error");
                    // to upper layer
                    cur = thisLayer->candidate;
                    thisLayer->candidate = nullptr;
                    last = thisLayer->tailNode;
                    DEBUG_PRINT_LOCAL("adding node to layers[%d]:"
                            "head_off:%d,tail_off:%d",
                            i + 1, thisLayer->head_off, thisLayer->tail_off);
                    continue;
                } else {
                    if ( thisLayer->tail_off % SkipGap == 1 ) {
                        MUST_TRUE(thisLayer->candidate == nullptr, "");
                        thisLayer->candidate = thisLayer->tailNode;
                    }
                    break;
                }
            }
        } else {
            if ( tail_off % SkipGap == 1 ) {
                MUST_TRUE(candidate == nullptr, "");
                candidate = tail;
            }
        }
    }
        
    struct SkipNode {
        void *downNode = nullptr;
        void *lastDownNode = nullptr;
        std::unique_ptr<SkipNode> nextNode;
    };
    struct Layer {
        int head_off = 0;
        int tail_off = 0;
        std::unique_ptr<SkipNode> headNode;
        SkipNode *tailNode = nullptr;
        SkipNode *candidate = nullptr;

        void cleanup() {
            head_off = tail_off;
            headNode = nullptr;
            tailNode = nullptr;
            candidate = nullptr;
        }
    };
    /* 64 ^ 5 = 2 ^ 30, practically enough */
    std::array<Layer, 5> layers;
    T *head = nullptr;
    T *tail = nullptr;
    T *candidate = nullptr;

    int head_off = 0;
    int tail_off = 0;

};

#endif /* _SKIPLIST_HH_ */

