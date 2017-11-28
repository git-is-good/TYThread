#ifndef _SKIPLIST_HH_
#define _SKIPLIST_HH_

#include "debug.hh"
#include "util.hh"
#include "Spinlock.hh"

#include <mutex>
#include <array>
#include <memory>
#include <utility>
#include <type_traits>

//#define ENABLE_DEBUG_LOCAL
#include "debug_local_begin.hh"

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
    template<class T, class LockType, int SkipGap, int NLayers>
    friend class Skiplist;
    Derived     *next;
};

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

    Layer() = default;

    Layer(Layer &&other) {
        _handleMove(std::move(other));
    }

    Layer &operator=(Layer &&other) {
        _handleMove(std::move(other));

        return *this;
    }

    void _handleMove(Layer &&other) {
        head_off = other.head_off;
        tail_off = other.tail_off;

        other.head_off = other.tail_off;
        headNode = std::move(other.headNode);
        tailNode = other.tailNode;
        candidate = other.candidate;

        other.tailNode = other.candidate = nullptr;
    }
};

#define ROUND_DOWN_IF_NOT_MULLIPLE(n, base) \
    ((n) % (base) == 0 ? -1 : (n) - (n) % (base))

template<class T, class LockType = Spinlock, int SkipGap = 64, int NLayers = 4>
class Skiplist : public NonCopyable {
public:
    using ref_ptr_type = DerivedRefPtr<T>;

    void enqueue(DerivedRefPtr<T> const &ptr) {
        std::lock_guard<LockType>   _(mut_);
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
        std::lock_guard<LockType>   _(mut_);
        if ( head ) {
            tail->next = ptr.get();
            tail = tail->next;
        } else {
            head = tail = ptr.get();
        }
        tail->next = nullptr;
        ptr.set(nullptr);
        _update_after_enque();
    }

    DerivedRefPtr<T> dequeue() {
        std::lock_guard<LockType>   _(mut_);
        return std::move(dequeue_());
    }

    DerivedRefPtr<T> dequeue_() {
        if ( !head ) {
            return std::move(DerivedRefPtr<T>{});
        }
        T *t = head;
        head = head->next;

        /* defensive */
        t->next = nullptr;
        DerivedRefPtr<T> res;
        res.set(t);

        if ( t == candidate ) {
            candidate = nullptr;
        }
        // this SkipNode is no longer full
        if ( ++head_off % SkipGap == 1 && layers[0].headNode ) {
            MUST_TRUE((void*)layers[0].headNode->downNode == t, "");

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
                if ( ++thisLayer->head_off % SkipGap == 1 &&
                        i + 1 < layers.size() && layers[i + 1].headNode ) {
                    MUST_TRUE(layers[i + 1].headNode->downNode == oldHead, "");
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

    std::unique_ptr<Skiplist<T>> dequeue_half_() {
        std::lock_guard<LockType>   _(mut_);

        if ( !head ) {
            return nullptr;
        }
        std::unique_ptr<Skiplist> res = std::make_unique<Skiplist<T>>();

        int toGiveAway = (tail_off - head_off + 1) / 2;
        for ( int i = 0; i < toGiveAway; ++i ) {
            res->enqueue(dequeue_());
        }
        return std::move(res);
    }

    std::unique_ptr<Skiplist<T>> dequeue_half() {
        enum {
            UpperCleanuped,
            SingleNodePreserved,
            Normal,
        };
        std::lock_guard<LockType>   _(mut_);
        int layerToSplit = layers.size();
        int upperState = Normal;
        void *thatDownNode = nullptr;
        std::unique_ptr<Skiplist> res;

        while ( --layerToSplit >= 0 ) {
            int sz = layers[layerToSplit].tail_off - layers[layerToSplit].head_off;
            if ( sz > 1 ) {
                /* this layer has at least 2 nodes, split it */
                break;
            } else if ( sz == 1 ) {
                /* this non-empty layer is useless */

                /* tricky situation, cannot simply delete this node:
                 *
                 *  |---------------------------------|
                 *  |   *       *       *       *     |
                 *  |   |       |       |       |     |
                 *  |   ****    ****    ****    ****  |  ****    ****    ****
                 *  |---------------------------------|
                 *  
                 *  If in this case, the whole node shoule be gived away
                 *  In other cases, we can delete this node
                 */
                
                void *downHead;
                int nextCount;
                if ( layerToSplit == 0 ) {
                    downHead = head;
                    nextCount = tail_off - head_off;
                } else {
                    downHead = layers[layerToSplit - 1].headNode.get();
                    nextCount = layers[layerToSplit - 1].tail_off - layers[layerToSplit - 1].head_off;
                }
                if ( layers[layerToSplit].headNode->downNode == downHead && nextCount == 2 * SkipGap - 1 ) {
                    DEBUG_PRINT_LOCAL("a single-noded layer %d, SingleNodePreserved", layerToSplit);
                    /* handle the case above */
                    res = std::make_unique<Skiplist>();
                    res->layers[layerToSplit] = std::move(layers[layerToSplit]);
                    upperState = SingleNodePreserved;
                } else {
                    DEBUG_PRINT_LOCAL("a single-noded layer %d, UpperCleanuped", layerToSplit);
                    thatDownNode = layers[layerToSplit].headNode->downNode;
                    layers[layerToSplit].cleanup();
                    upperState = UpperCleanuped;
                }
            }
        }

        if ( layerToSplit >= 0 ) {
            DEBUG_PRINT_LOCAL("layerToSplit found: %d", layerToSplit);

            if ( !res ) {
                res = std::make_unique<Skiplist>();
            }

            Layer *thisLayer = &layers[layerToSplit];
            int toGiveAway = (thisLayer->tail_off - thisLayer->head_off + 1) / 2;
                
            if ( upperState == UpperCleanuped ) {
                res->layers[layerToSplit].candidate = reinterpret_cast<SkipNode*>(thatDownNode);
            } else if ( upperState == Normal ) {
                int candpos = ROUND_DOWN_IF_NOT_MULLIPLE(thisLayer->tail_off, SkipGap);
                if ( candpos == -1 ) {
                    MUST_TRUE(thisLayer->candidate == nullptr, "thisLayer->tail_off:%d", thisLayer->tail_off);
                }
                if ( candpos != -1 && thisLayer->head_off <= candpos &&
                        thisLayer->head_off + toGiveAway > candpos ) {
                    // candidate is stolen
                    res->layers[layerToSplit].candidate = thisLayer->candidate;
                    thisLayer->candidate = nullptr;
                }
            }

            SkipNode *lastToGiveAway = thisLayer->headNode.get();
            for ( int i = 0; i < toGiveAway - 1; i++ ) {
                lastToGiveAway = lastToGiveAway->nextNode.get();
            }
            SkipNode *cur = lastToGiveAway;

            int toGiveAwayCurLayer = toGiveAway;
            for ( int i = layerToSplit; i >= 0; --i ) {
                std::unique_ptr<SkipNode> cur_k = std::move(cur->nextNode);
                MUST_TRUE(cur_k != nullptr, "");

                /* how many next layer nodes to give away */
                int toGiveAwayNextLayer;
                int tmp_head_off = i != 0 ? layers[i - 1].head_off : head_off;
//                if ( tmp_head_off % SkipGap == 0 ) {
//                    toGiveAwayNextLayer = toGiveAwayCurLayer * SkipGap;
//                } else {
//                    toGiveAwayNextLayer = (1 + toGiveAwayCurLayer) * SkipGap - tmp_head_off % SkipGap;
//                }

                toGiveAwayNextLayer = (1 + toGiveAwayCurLayer) * SkipGap - (tmp_head_off + SkipGap - 1) % SkipGap - 1;

                res->layers[i].headNode = std::move(layers[i].headNode);
                layers[i].headNode = std::move(cur_k);

                res->layers[i].tailNode = cur;
                /* layers[i].tailNode no change */

                res->layers[i].head_off = layers[i].head_off;
                res->layers[i].tail_off = layers[i].head_off + toGiveAwayCurLayer;
                layers[i].head_off = res->layers[i].tail_off;
                /* layers[i].tail_off no change */

                DEBUG_PRINT_LOCAL("res->layers[%d].head_off:%d,tail_off:%d",
                        i, res->layers[i].head_off, res->layers[i].tail_off);

                DEBUG_PRINT_LOCAL("layers[%d].head_off:%d,tail_off:%d",
                        i, layers[i].head_off, layers[i].tail_off);

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
            head_off = res->tail_off;

            DEBUG_PRINT_LOCAL("final: res->head_off:%d,res->tail_off:%d",
                    res->head_off, res->tail_off);

            DEBUG_PRINT_LOCAL("final: head_off:%d,tail_off:%d",
                    head_off, tail_off);

            return std::move(res);
        } else {
            /* split the raw data */

            int toGiveAway = (tail_off - head_off + 1) / 2;
            if ( toGiveAway == 0 ) {
                MUST_TRUE(res == nullptr, "");
                DEBUG_PRINT_LOCAL("Nothing to give away");
                return nullptr;
            }

            if ( !res ) {
                res = std::make_unique<Skiplist>();
            }

            if ( upperState == UpperCleanuped ) {
                res->candidate = reinterpret_cast<T*>(thatDownNode);
            } else if ( upperState == Normal ) {
                int candpos = ROUND_DOWN_IF_NOT_MULLIPLE(tail_off, SkipGap);
                if ( candpos != -1 && head_off <= candpos && head_off + toGiveAway > candpos ) {
                    // candidate is stolen
                    res->candidate = candidate;
                    candidate = nullptr;
                }
            }

            DEBUG_PRINT_LOCAL("Give away raw data: toGiveAway:%d,head_off:%d,tail_off:%d", toGiveAway, head_off, tail_off);
            T* lastToGiveAway = head;
            for ( int i = 0; i < toGiveAway - 1; i++ ) {
                lastToGiveAway = lastToGiveAway->next;
            }

            res->head = head;
            head = lastToGiveAway->next;
            lastToGiveAway->next = nullptr;

            res->tail = lastToGiveAway;
            res->head_off = head_off;
            res->tail_off = head_off + toGiveAway;
            head_off = res->tail_off;

            DEBUG_PRINT_LOCAL("split raw data: res->head_off:%d,res->tail_off:%d",
                    res->head_off, res->tail_off);

            DEBUG_PRINT_LOCAL("split raw data: head_off:%d,tail_off:%d",
                    head_off, tail_off);
            return std::move(res);
        }
    }

    void replace(std::unique_ptr<Skiplist> &ptr) {
        std::lock_guard<LockType>   _(mut_);
        MUST_TRUE(head_off == tail_off, "try to replace a non-empry Skiplist: "
                "head_off:%d,tail_off:%d", head_off, tail_off);

        layers = std::move(ptr->layers);
        head = ptr->head;
        tail = ptr->tail;
        candidate = ptr->candidate;
        head_off = ptr->head_off;
        tail_off = ptr->tail_off;

        ptr->head = nullptr;
        ptr->tail = nullptr;
        ptr->candidate = nullptr;
        ptr->head_off = 0;
        ptr->tail_off = 0;
    }

    ~Skiplist() {
        if ( head ) {
            MUST_TRUE(tail && tail_off > head_off, "tail:%p,head_off:%d,tail_off:%d", tail, head_off, tail_off);
            DEBUG_PRINT_LOCAL("Destructor of Skiplist called when head_off:%d,tail_off:%d",
                   head_off, tail_off);
            while ( head != nullptr ) {
                ref_ptr_type::decrease(head);
                head = head->next;
            }
            ref_ptr_type::decrease(head);
        }
    }

    std::size_t size() const {
        std::lock_guard<LockType> _(mut_);
        return tail_off - head_off;
    }
private:
    // invariant: every SkipNode is full
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
                    MUST_TRUE(thisLayer->tail_off - thisLayer->head_off >= SkipGap,
                            "head_off:%d,tail_off:%d", thisLayer->head_off, thisLayer->tail_off);
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
        
    /* 64 ^ 5 = 2 ^ 30, practically enough */
    std::array<Layer, NLayers> layers;
    T *head = nullptr;
    T *tail = nullptr;
    T *candidate = nullptr;

    int head_off = 0;
    int tail_off = 0;

    mutable LockType mut_;
};

#include "debug_local_end.hh"

#endif /* _SKIPLIST_HH_ */

