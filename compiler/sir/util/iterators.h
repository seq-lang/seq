#pragma once

#include <iterator>
#include <memory>
#include <type_traits>

namespace seq {
namespace ir {
namespace util {

template <typename It, typename Func> class function_iterator_adaptor {
private:
  It internal;
  Func f;

public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::remove_reference<decltype(f(*internal))>::type;
  using reference = void;
  using pointer = void;
  using difference_type = typename std::iterator_traits<It>::difference_type;

  function_iterator_adaptor(It internal, Func &&f)
      : internal(std::move(internal)), f(std::move(f)) {}

  decltype(auto) operator*() { return f(*internal); }
  function_iterator_adaptor &operator++() {
    internal++;
    return *this;
  }
  function_iterator_adaptor operator++(int) {
    function_iterator_adaptor<It, Func> copy(*this);
    internal++;
    return copy;
  }

  bool operator==(const function_iterator_adaptor<It, Func> &other) {
    return other.internal == internal;
  }

  bool operator!=(const function_iterator_adaptor<It, Func> &other) {
    return other.internal != internal;
  }
};

template <typename It> auto dereference_adaptor(It it) {
  auto f = [](auto &v) -> auto & { return *v; };
  return function_iterator_adaptor<It, decltype(f)>(it, std::move(f));
}
} // namespace util
} // namespace ir
} // namespace seq
