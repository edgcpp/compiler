//options_all:--c++11
//type:fn
template <class T>
struct vector {};

using result_type = unsigned int;
vector<result_type> y;

void f(vector<unsigned int>);

int main() {
    f(0);
}
