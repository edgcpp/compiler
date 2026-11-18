//type:cp
//options_all:
template<typename T>
struct templ {
	templ(T t) {}
};

template<typename T>
struct test {
	struct FArguments {
		FArguments() {}
	};

	templ<FArguments> f() {
		return templ<FArguments>(FArguments{});
	}
};

int main() {
	test<double> t;
}
