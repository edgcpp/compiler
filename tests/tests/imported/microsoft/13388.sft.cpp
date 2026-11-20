//options_all:--microsoft --c++14
//type:fn
template <int... Pack>
struct list
{
static int const values[];
};

template <int... Pack>
int const list<Pack...>::values[] = { Pack... };

int main()
{
list<>::values; // Error expected here, cannot create an array with 0 elements.
}
