#include <utility>
#include <functional>

namespace co {
    template<class T>
    class channel {
    public:
        friend void operator<<(channel ch, T value) {
            ch.value = value;
        }
        friend void operator<<(T &store, channel ch) {
            store = ch.value;
        }
    private:
        T value;
    };

};

template<class... Args>
void
multiplex(std::pair<co::channel<Args>, std::function<void(Args)>> args...)
{

}
