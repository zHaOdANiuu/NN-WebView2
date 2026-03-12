// module;

// export module function_traits;

// Fuck cplusplus, it's 2026!!! 6 year, 6 year!!!
// It's been 6 whole years!!! Why is the module such a loser!!!
// clangd is actually statically identified.
// WTF, am I living in 1998? Working in the terminal with vim and ctags. Holy shit!!!
//  Alright, life goes on... #include <fuck.hpp>

#pragma once

#include <tuple>

template <typename T> struct FunctionTraits {};

template <typename T, typename... Args> struct FunctionTraits<T(Args...)> {
  using returnType        = T;
  using argsType          = std::tuple<Args...>;
  using memFuncArgsType = std::tuple<Args...>;

  static constexpr std::size_t argsCount = std::tuple_size_v<argsType>;
};

template <typename Ty>
  requires(std::is_class_v<Ty> && std::is_void_v<std::void_t<decltype(&Ty::operator())>>)
struct FunctionTraits<Ty> : FunctionTraits<std::remove_pointer_t<decltype(&Ty::operator())>> {};

template <typename _Class, typename T>
struct FunctionTraits<_Class T::*> : FunctionTraits<_Class (T::*)()> {};

template <typename Fn, typename... Args>
struct FunctionTraits<Fn (*)(Args...)> : FunctionTraits<Fn(Args...)> {
  using argsType = std::tuple<Args...>;
};

template <typename Fn, typename T, typename... Args>
struct FunctionTraits<Fn (T::*)(Args...)> : FunctionTraits<Fn(T*, Args...)> {
  using memFuncArgsType = std::tuple<Args...>;
};

#define VAR_FN(ext)                        \
  template <typename Fn, typename... Args> \
  struct FunctionTraits<Fn(Args...) ext> : FunctionTraits<Fn(Args...)> {};

VAR_FN(&);

VAR_FN(&&);

VAR_FN(const);

VAR_FN(const&);

VAR_FN(noexcept);

VAR_FN(& noexcept);

VAR_FN(&& noexcept);

VAR_FN(const noexcept);

VAR_FN(const& noexcept);

#define VAR_MEMBER_FN(ext)                                 \
  template <typename _Class, typename T, typename... Args> \
  struct FunctionTraits<_Class (T::*)(Args...) ext>        \
      : FunctionTraits<_Class (T::*)(Args...)> {};

VAR_MEMBER_FN(&);

VAR_MEMBER_FN(&&);

VAR_MEMBER_FN(const);

VAR_MEMBER_FN(const&);

VAR_MEMBER_FN(noexcept);

VAR_MEMBER_FN(& noexcept);

VAR_MEMBER_FN(&& noexcept);

VAR_MEMBER_FN(const noexcept);

VAR_MEMBER_FN(const& noexcept);
