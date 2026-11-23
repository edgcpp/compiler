/**
 * @file stl_torture_44.cpp
 * @brief 44-Header C++20/C++23 Standard Library Torture Translation Unit.
 * @details Includes all 44 standard library headers required by Section 9.2 of
 * TODO_PLAN.md in a single C++20/C++23 translation unit and exercises STL
 * containers, algorithms, ranges, formatting, streams, filesystem, threads,
 * futures, shared_mutex, atomics, smart pointers, std::variant, std::any,
 * std::optional, std::function, regex, valarray, complex, bit operations,
 * coroutines, source_location, and exception handling across ABI boundaries.
 */

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <concepts>
#include <coroutine>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <regex>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

template <std::integral T>
static T pop_and_rot(T v) {
  return static_cast<T>(std::popcount(static_cast<std::make_unsigned_t<T>>(v)));
}

struct MiniCoro {
  struct promise_type {
    int val = 0;
    MiniCoro get_return_object() {
      return MiniCoro{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int x) noexcept { val = x; }
    void unhandled_exception() noexcept {}
  };
  std::coroutine_handle<promise_type> h;
  ~MiniCoro() {
    if (h) h.destroy();
  }
};

static MiniCoro make_coro(int x) {
  co_return x * 3 + 1;
}

static int probe_line(std::source_location loc = std::source_location::current()) {
  return loc.line() > 0 ? 1 : 0;
}

int main() {
  // 1. Sequence & associative containers + algorithms + ranges + span
  std::array<int, 6> arr = {9, 2, 7, 4, 6, 1};
  std::vector<int> vec(arr.begin(), arr.end());
  std::sort(vec.begin(), vec.end());
  std::span<const int> sp(vec);
  int even_sq_sum = 0;
  for (int x : sp | std::views::filter([](int v) { return v % 2 == 0; })) {
    even_sq_sum += x * x; // 4 + 16 + 36 = 56
  }

  std::deque<int> dq = {10, 20};
  dq.push_front(5);
  std::list<int> lst(dq.begin(), dq.end());
  int lst_sum = std::accumulate(lst.begin(), lst.end(), 0); // 35

  std::map<std::string, int> mp = {{"alpha", 3}, {"beta", 8}};
  std::unordered_map<std::string_view, int> ump = {{"k1", 11}, {"k2", 13}};
  std::set<int> st = {4, 2, 4, 9};
  std::unordered_set<int> ust = {7, 7, 8};
  std::priority_queue<int> pq;
  pq.push(12);
  pq.push(45);
  pq.push(19);

  // 2. Smart pointers, optional, variant, any, tuple, typeindex, function
  auto up = std::make_unique<int>(25);
  auto shp = std::make_shared<std::pair<int, int>>(*up, 15);
  std::optional<int> opt = shp->first + shp->second; // 40
  std::variant<int, std::string> var = std::string("hello");
  int var_len = std::visit(
      [](const auto& item) -> int {
        using V = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<V, int>) {
          return item;
        } else {
          return static_cast<int>(item.size());
        }
      },
      var); // 5

  std::any a_val = 33;
  int any_extracted = std::any_cast<int>(a_val);
  std::map<std::type_index, int> timap;
  timap[std::type_index(typeid(int))] = 7;
  std::function<int(int, int)> fn = [](int x, int y) { return x * y + 2; };

  // 3. Formatting, streams, regex, valarray, complex, bit, random, chrono, fs
  std::string formatted = std::format("{}-{}-{}", vec.front(), vec.back(), *opt);
  std::ostringstream oss;
  oss << formatted << ":" << fn(3, 4);

  std::regex re("([0-9]+)-([0-9]+)-([0-9]+):([0-9]+)");
  std::smatch m;
  std::string s_out = oss.str();
  bool re_ok = std::regex_match(s_out, m, re);
  int re_sum = re_ok ? (std::stoi(m[1].str()) + std::stoi(m[4].str())) : -1; // 1 + 14 = 15

  std::valarray<int> va = {2, 4, 6, 8};
  std::valarray<int> vb = va * 2; // sum = 40
  std::complex<double> cz(3.0, 4.0);
  int c_norm = static_cast<int>(std::round(std::abs(cz))); // 5
  int bits = pop_and_rot(0b101101); // 4

  std::mt19937 rng(12345u);
  int rand_det = static_cast<int>(rng() % 100u);

  auto t0 = std::chrono::steady_clock::now();
  std::filesystem::path p = std::filesystem::path("dir") / "sub" / "file.txt";
  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t0)
                .count();

  // 4. Threads, future, mutex, shared_mutex, atomics, exceptions, coroutines
  std::atomic<int> atom_acc{0};
  std::shared_mutex rw_mtx;
  std::mutex mtx;
  int guarded_total = 0;

  auto fut = std::async(std::launch::async, [&]() {
    std::shared_lock<std::shared_mutex> rlk(rw_mtx);
    return 100;
  });

  std::vector<std::thread> workers;
  for (int i = 1; i <= 4; ++i) {
    workers.emplace_back([&, i]() {
      atom_acc.fetch_add(i, std::memory_order_relaxed);
      try {
        if (i % 2 == 0) {
          throw std::domain_error("even");
        }
      } catch (const std::logic_error&) {
        std::lock_guard<std::mutex> lk(mtx);
        guarded_total += i * 10;
      }
    });
  }
  for (auto& w : workers) w.join();
  int async_v = fut.get();

  MiniCoro mc = make_coro(7); // 22

  std::cout << even_sq_sum << " " << lst_sum << " " << mp["beta"] << " "
            << ump["k2"] << " " << st.size() << " " << ust.size() << " "
            << pq.top() << " " << var_len << " " << any_extracted << " "
            << timap[std::type_index(typeid(int))] << " " << re_sum << " "
            << vb.sum() << " " << c_norm << " " << bits << " " << rand_det
            << " " << p.filename().string() << " " << (dt >= 0 ? 1 : 0) << " "
            << atom_acc.load() << " " << guarded_total << " " << async_v << " "
            << mc.h.promise().val << " " << probe_line() << '\n';
  return 0;
}
