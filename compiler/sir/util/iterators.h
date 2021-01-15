#pragma once

#include <iterator>
#include <memory>
#include <type_traits>

namespace seq {
namespace ir {
namespace util {

template <typename It, typename DereferenceFunc, typename MemberFunc>
struct function_iterator_adaptor {
  It internal;
  DereferenceFunc d;
  MemberFunc m;

  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::remove_reference<decltype(d(*internal))>::type;
  using reference = void;
  using pointer = void;
  using difference_type = typename std::iterator_traits<It>::difference_type;

  function_iterator_adaptor(It internal, DereferenceFunc &&d, MemberFunc &&m)
      : internal(std::move(internal)), d(std::move(d)), m(std::move(m)) {}

  decltype(auto) operator*() { return d(*internal); }
  decltype(auto) operator->() { return m(*internal); }

  function_iterator_adaptor &operator++() {
    internal++;
    return *this;
  }
  function_iterator_adaptor operator++(int) {
    function_iterator_adaptor<It, DereferenceFunc, MemberFunc> copy(*this);
    internal++;
    return copy;
  }

  template <typename OtherIt, typename OtherDereferenceFunc, typename OtherMemberFunc>
  bool operator==(const function_iterator_adaptor<OtherIt, OtherDereferenceFunc,
                                                  OtherMemberFunc> &other) const {
    return other.internal == internal;
  }

  template <typename OtherIt, typename OtherDereferenceFunc, typename OtherMemberFunc>
  bool operator!=(const function_iterator_adaptor<OtherIt, OtherDereferenceFunc,
                                                  OtherMemberFunc> &other) const {
    return other.internal != internal;
  }
};

template <typename It> auto dereference_adaptor(It it) {
  auto f = [](const auto &v) -> auto & { return *v; };
  auto m = [](const auto &v) -> auto { return v.get(); };
  return function_iterator_adaptor<It, decltype(f), decltype(m)>(it, std::move(f),
                                                                 std::move(m));
}

template <typename It> auto raw_ptr_adaptor(It it) {
  auto f = [](auto &v) -> auto * { return v.get(); };
  auto m = [](auto &v) -> auto * { return v.get(); };
  return function_iterator_adaptor<It, decltype(f), decltype(m)>(it, std::move(f),
                                                                 std::move(m));
}

template <typename It> auto const_raw_ptr_adaptor(It it) {
  auto f = [](auto &v) -> const auto * { return v.get(); };
  auto m = [](auto &v) -> const auto * { return v.get(); };
  return function_iterator_adaptor<It, decltype(f), decltype(m)>(it, std::move(f),
                                                                 std::move(m));
}

template <typename It> auto map_key_adaptor(It it) {
  auto f = [](auto &v) -> auto & { return v.first; };
  auto m = [](auto &v) -> auto & { return v.first; };
  return function_iterator_adaptor<It, decltype(f), decltype(m)>(it, std::move(f),
                                                                 std::move(m));
}

template <typename It> auto const_map_key_adaptor(It it) {
  auto f = [](auto &v) -> const auto & { return v.first; };
  auto m = [](auto &v) -> const auto & { return v.first; };
  return function_iterator_adaptor<It, decltype(f), decltype(m)>(it, std::move(f),
                                                                 std::move(m));
}

} // namespace util
} // namespace ir
} // namespace seq
