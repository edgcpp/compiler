//type:rp
//require:THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED 1
//options:--c++17 --g++ --gnu_version=150000:--c++17 --g++ --gnu_version=150000 -tused:--c++17 --clang --clang_version=190100
//linker_options:-lpthread
// Each instance of a "__thread" variable template has thread storage
// duration, like an instance of a "thread_local" variable template.
// Previously the instances were ordinary static variables, shared by all
// threads.  The internal-linkage cases (and the non-template "plain" and
// "u") also check that the generated C defines each "__thread" variable
// once.  No headers are included; the pthread_t typedef below matches glibc
// on x86 targets.
extern "C" {
typedef unsigned long a_pthread;
int pthread_create(a_pthread *, const void *, void *(*)(void *), void *);
int pthread_join(a_pthread, void **);
}

template<class T> __thread T tv = 10;
template<class T> __thread T *tv<T*> = nullptr;   // Partial specialization.
template<> long tv<long> = 10;    // Explicit specialization: not thread-local.
template<class T> thread_local T tl = 10;         // Control.
struct S { template<class T> static __thread T m; };
template<class T> __thread T S::m = 10;
template<class U> struct C { template<class T> static __thread T cm; };
template<class U> template<class T> __thread T C<U>::cm = 10;
namespace { template<class T> __thread T anon = 10; }
template<class T> static __thread T st = 10;
template<class T> static __thread T st_noinit;
template<class T> inline __thread T inl = 10;
template<class T> T nt = 10;
template<> __thread int nt<int> = 10;   // Explicit specialization.
static __thread int plain = 10;
static __thread int u;

const int n_vars = 11;
static const int initial[n_vars] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0};

// Get the addresses of the int variables in the calling thread.
static void addresses(int **a)
{
  a[0] = &tv<int>;          a[1] = &tl<int>;    a[2] = &S::m<int>;
  a[3] = &C<int>::cm<int>;  a[4] = &anon<int>;  a[5] = &st<int>;
  a[6] = &inl<int>;         a[7] = &plain;      a[8] = &nt<int>;
  a[9] = &st_noinit<int>;   a[10] = &u;
}

static int *main_addrs[n_vars], *worker_addrs[n_vars];
static int worker_saw[n_vars];
static int **main_ptr_addr, **worker_ptr_addr;
static int *worker_saw_ptr;
static long *worker_long_addr;
static long worker_saw_long;

static void *worker(void *)
{
  addresses(worker_addrs);
  for (int i = 0; i < n_vars; ++i) {
    worker_saw[i] = *worker_addrs[i];
    *worker_addrs[i] = 1;
  }  // for
  worker_ptr_addr = &tv<int*>;
  worker_saw_ptr = tv<int*>;
  tv<int*> = &tv<int>;
  worker_long_addr = &tv<long>;
  worker_saw_long = tv<long>;
  return nullptr;
}

int main()
{
  int fails = 0;
  a_pthread t;
  addresses(main_addrs);
  for (int i = 0; i < n_vars; ++i) *main_addrs[i] = 99;
  main_ptr_addr = &tv<int*>;
  tv<int*> = main_addrs[0];
  tv<long> = 99;
  if (pthread_create(&t, nullptr, worker, nullptr) != 0) return 100;
  pthread_join(t, nullptr);
  for (int i = 0; i < n_vars; ++i) {
    // Each thread has its own object, initialized from the template.
    if (worker_addrs[i] == main_addrs[i]) fails |= 1;
    if (worker_saw[i] != initial[i]) fails |= 2;
    if (*main_addrs[i] != 99) fails |= 4;
  }  // for
  if (worker_ptr_addr == main_ptr_addr || worker_saw_ptr != nullptr ||
      tv<int*> != main_addrs[0]) {
    fails |= 8;
  }  // if
  // The explicit specialization tv<long> is shared by the threads.
  if (worker_long_addr != &tv<long> || worker_saw_long != 99) fails |= 16;
  return fails;
}
