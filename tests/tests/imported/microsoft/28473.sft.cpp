//type:cp
//options_all:--ms_c++20 --microsoft_v 1949
struct vector;
struct iterator;

struct container_proxy {
    const vector* mycont  = nullptr;
    iterator* myfirstiter = nullptr;
};

struct iterator {
    constexpr iterator(int* parg, const vector* pvector) : ptr(parg) {
        adopt(pvector);
    }

    constexpr iterator(const iterator& right) {
        *this = right;
    }

    constexpr iterator& operator=(const iterator& right) {
        if (myproxy != right.myproxy) {
            if (right.myproxy) {
                adopt(right.myproxy->mycont);
            } else {
                orphan_me();
            }
        }
        ptr = right.ptr;
        return *this;
    }

    constexpr ~iterator() {
        orphan_me();
    }

    constexpr void adopt(const vector* parent);

    constexpr void orphan_me() {
        if (!myproxy) {
            return;
        }

        for (iterator** pnext = &myproxy->myfirstiter;; pnext = &(*pnext)->mynextiter) {
            if (*pnext == this) {
                *pnext = mynextiter;
                break;
            }
        }

        myproxy = nullptr;
    }

    mutable container_proxy* myproxy = nullptr;
    mutable iterator* mynextiter     = nullptr;

    int* ptr = nullptr;
};

struct vector {
    constexpr vector() : myproxy{new container_proxy{this}} {}

    vector(const vector&)            = delete;
    vector& operator=(const vector&) = delete;

    constexpr ~vector() {
        auto pnext           = myproxy->myfirstiter;
        myproxy->myfirstiter = nullptr;
        for (; pnext; pnext = pnext->mynextiter) {
            pnext->myproxy = nullptr;
        }

        delete myproxy;
    }

    constexpr iterator cbegin() const {
        return iterator(myfirst, this);
    }

    container_proxy* myproxy = nullptr;

    int* myfirst = nullptr;
    int* mylast  = nullptr;
    int* myend   = nullptr;
};

constexpr void iterator::adopt(const vector* parent) {
    if (!parent) {
        orphan_me();
        return;
    }

    container_proxy* parent_proxy = parent->myproxy;
    if (myproxy != parent_proxy) {
        if (myproxy) {
            orphan_me();
        }
        mynextiter                = parent_proxy->myfirstiter;
        parent_proxy->myfirstiter = this;
        myproxy                   = parent_proxy;
    }
}

constexpr iterator operator+(const iterator& it, int) {
    return it;
}

template <class InIt>
constexpr InIt find(InIt first) {
    return true ? first + 0 : first + 0;
}

constexpr bool test_find() {
    vector input_true;
    const auto result_true = find(input_true.cbegin());
    return true;
}

static_assert(test_find());
