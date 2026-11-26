//type:fp
//options_all:--ms_permissive
int main()
{
	struct A{};

	static_assert(!__is_convertible_to(A, A&),"__is_convertible_to was true");
}
