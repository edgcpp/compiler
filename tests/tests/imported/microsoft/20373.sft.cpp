//options_all:--microsoft_v 2000
template <typename TT>
struct Base {
              Base(int) {}
              Base(const TT&) {}
};

struct cc : public Base<cc> {
              using Base<cc>::Base;
};

int main() {
              cc c = 0;
}
