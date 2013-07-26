//
// C++ templates are Turing-complete. One way to show that would be to 
// implement a system that can simulate any Turing machine (for that, see
// turing-machine.cpp). But Turing machines are not the only formalism out
// there -- mu-recursive functions are one such formalism. And that's what
// we're going to do here.
//
// Compile with
//   $CXX -Wall -Wextra -std=c++11 -pedantic mu-recursive-functions.cpp -o mu-recursive-functions
// I've tested this with $CXX = g++ 4.8.1 and clang++ 3.3.
//


#include <iostream>

//
// Elementary functions:
//

// The zero function:
// zero(x_1, x_2, ..., x_n) = 0
template <unsigned... Dummy>
struct zero { 
  static constexpr unsigned value = 0; 
};

// The successor function:
// successor(x) = x + 1
template <unsigned X>
struct successor {
  static constexpr unsigned value = X + 1; 
};

// The projection function scheme:
// For all natural I, K, such that 1 <= I <= K:
//   projection_I(x_1, x_2, ..., x_K) = x_I
template <unsigned I>
struct projection {
  template <unsigned First, unsigned... Rest>
  struct fun { 
    static_assert(sizeof...(Rest) + 1 >= I, "Invalid projection");

    static constexpr unsigned 
    value = projection<I - 1>::template fun<Rest...>::value; 
  };
};

template <>
struct projection<0> {
  template <unsigned X, unsigned... Rest>
  struct fun {
    static constexpr unsigned value = X; 
  };
};

//
// Operators:
//

// Substitution operator:
// Given m-ary function h(x_1, ..., x_m) and m k-ary functions
// g_1(x_1, ..., x_k), ..., g_m(x_1, ..., x_k) return a k-ary function f:
// substitution(h, g_1, ..., g_m) =
//   = f(x_1, ..., x_k) = 
//   = h(g_1(x_1, ..., x_k), g_2(x_1, ..., x_k), ..., g_m(x_1, ..., x_k))
template <
  template <unsigned...> class H,
  template <unsigned...> class... Gs
>
struct substitution {
  template <unsigned... Xs>
  struct fun {
    static constexpr unsigned
    value = H<Gs<Xs...>::value...>::value;
  };
};

// Promitive recursion operator:
// Given k-ary function g(x_1, ..., x_k) and (k + 2)-ary function 
// h(y, z, x_1, ..., x_k), return a (k + 1)-ary function f:
// recursion(g, h) =
//   = f(y, x_1, ..., x_k)
// such that
//   f(0, x_1, ..., x_k)     = g(x_1, ..., x_k),
//   f(y + 1, x_1, ..., x_k) = h(y, f(y, x_1, ..., x_k), x_1, ..., x_k)
namespace detail {
  template <
    template <unsigned...> class G,
    template <unsigned...> class H,
    int Y, int... Xs
  >
  struct recursion_helper {
    static constexpr unsigned
    value = H<Y - 1, recursion_helper<G, H, Y - 1, Xs...>::value, Xs...>::value;
  };

  template <
    template <unsigned...> class G,
    template <unsigned...> class H,
    int... Xs
  >
  struct recursion_helper<G, H, 0, Xs...> {
    static constexpr unsigned
    value = G<Xs...>::value;
  };
} // end namespace detail

template <
  template <unsigned...> class G,
  template <unsigned...> class H
>
struct recursion {
  template <int Y, int... Xs>
  using fun = detail::recursion_helper<G, H, Y, Xs...>;
};

// Minimisation operator:
// Given a (k + 1)-ary function f(y, x_1, ..., x_k) return a k-ary function 
// h(x_1, ..., x_k):
//   h(x_1, ..., x_k) = z
// such that f(i, x_1, ..., x_k) > 0 for all i < z and
//           f(z, x_1, ..., x_k) = 0.
namespace detail {
  template <
    unsigned Z,
    template <unsigned...> class F,
    bool Final,
    unsigned... Xs
  >
  struct minimisation_helper {
    static constexpr unsigned
    value = minimisation_helper<
      Z + 1, F, F<Z + 1, Xs...>::value == 0, Xs...
    >::value;
  };

  template <
    unsigned Z,
    template <unsigned...> class F,
    unsigned... Xs
  >
  struct minimisation_helper<Z, F, true, Xs...> {
    static constexpr unsigned value = Z;
  };
} // end namespace detail

template <
  template <unsigned...> class F
>
struct minimisation {
  template <unsigned... Xs>
  struct fun {
    static constexpr unsigned 
    value = detail::minimisation_helper<
      0, F, F<0, Xs...>::value == 0, Xs...
    >::value;
  };
};

// 
// Some derived functions
//

// constant_N(x_1, ..., x_k) := N
template <unsigned N>
struct constant {
  template <unsigned... Xs>
  struct fun {
    static constexpr unsigned
    value = successor<constant<N - 1>::template fun<Xs...>::value>::value;
  };
};

template <>
struct constant<0> {
  template <unsigned... Xs>
  using fun = zero<Xs...>;
};

// sum(x, y) := x + y
template <unsigned X, unsigned Y>
using sum =
  recursion<
    projection<0>::fun,    // x = 0 => (y) |-> y
    substitution<          // x > 0 => (x - 1, sum(x - 1, y), y) |-> s(y)
      successor,
      projection<1>::fun
    >::fun
  >::fun<X, Y>;

// pred(x) := x - 1, if x > 0
// pred(x) := 0,     if x = 0
template <unsigned X>
using pred =
  recursion<
    zero,                 // x = 0 => (x) |-> 0
    projection<0>::fun    // x > 0 => (x - 1, pred(x - 1)) |-> x - 1
  >::fun<X>;

// sub1(x, y) := y - x, if x >= y
// sub1(x, y) := 0,     if x < y
template <unsigned X, unsigned Y>
using sub1 =
  recursion<
    projection<0>::fun,   // y = 0 => (x) |-> x
    // y > 0 => (y - 1, sub(y - 1, x), x) |-> pred(sub(y - 1, x))
    substitution<
      pred,
      projection<1>::fun
    >::fun
  >::fun<X, Y>;

// sub(x, y) := x - y, if x >= y
// sub(x, y) := 0,     if x < 0
template <unsigned X, unsigned Y>
using sub =
  // sub(x, y) = sub1(y, x)
  substitution<sub1, projection<1>::fun, projection<0>::fun>::fun<X, Y>;

// mul(x, y) := xy
template <unsigned X, unsigned Y>
using mul =
  recursion<
    zero,                 // x = 0 => (y) |-> 0
    // x > 0 => (x - 1, mul(x - 1, y), y) |-> sum(mul(x - 1, y), y)
    substitution<
      sum, projection<1>::fun, projection<2>::fun
    >::fun
  >::fun<X, Y>;

// sgn(x) := 0, if x = 0
// sgn(x) := 1, if x > 0
template <unsigned X>
using sgn =
  recursion<
    zero,
    constant<1>::fun
  >::fun<X>;

// cosgn(x) := 1, if x = 0
// cosgn(x) := 0, if x > 0
template <unsigned X>
using cosgn =
  recursion<
    constant<1>::fun,
    zero
  >::fun<X>;

// lt(x, y) := 1, if x < y
// lt(x, y) := 0, if x >= y
template <unsigned X, unsigned Y>
using lt =
  // lt(x, y) = sgn(y -' x)
  substitution<
    sgn,
    substitution<sub, projection<1>::fun, projection<0>::fun>::fun
  >::fun<X, Y>;

// gt(x, y) := 1, if x > y
// gt(x, y) := 0, if x <= y
template <unsigned X, unsigned Y>
using gt =
  // gt(x, y) = sgn(x -' y)
  substitution<sgn, sub>::fun<X, Y>;

// eq(x, y) := 1, if x = y
// eq(x, y) := 0, if x =/= y
template <unsigned X, unsigned Y>
using eq =
  // eq(x, y) = cosgn(lt(x, y) + gt(x, y))
  substitution<
    cosgn, substitution<sum, lt, gt>::fun
  >::fun<X, Y>;

// neq(x, y) := 0, if x = y
// neq(x, y) := 1, if x =/= y
template <unsigned X, unsigned Y>
using neq =
  // neq(x, y) = cosgn(eq(x, y))
  substitution<cosgn, eq>::fun<X, Y>;

// square(x) := x*x
template <unsigned X>
using square =
  substitution<
    mul, projection<0>::fun, projection<0>::fun
  >::fun<X>;

// sqrt(x) = z such that z^2 = x, if any such natural z exists
// sqrt(x) = undefined,           otherwise
template <unsigned X>
using sqrt =
  // sqrt(x) = mu_z [neq(square(z), x)]
  minimisation<
    substitution<
      neq,
      substitution<square, projection<0>::fun>::fun,
      projection<1>::fun
    >::fun
  >::fun<X>;

//
// Test:
//

int main() {
  std::cout << "5        = " << constant<5>::fun<>::value << '\n';
  std::cout << "2 + 3    = " << sum<2, 3>::value << '\n';
  std::cout << "2 -' 1   = " << pred<2>::value << '\n';
  std::cout << "0 -' 1   = " << pred<0>::value << '\n';
  std::cout << "8 -' 3   = " << sub<8, 3>::value << '\n';
  std::cout << "5 -' 9   = " << sub<5, 9>::value << '\n';
  std::cout << "2 * 4    = " << mul<2, 4>::value << '\n';
  std::cout << "3 * 0    = " << mul<3, 0>::value << '\n';
  std::cout << "0 * 9    = " << mul<0, 9>::value << '\n';
  std::cout << "9 * 25   = " << mul<9, 25>::value << '\n';
  std::cout << "sgn(0)   = " << sgn<0>::value << '\n';
  std::cout << "sgn(5)   = " << sgn<5>::value << '\n';
  std::cout << "2 < 3    = " << lt<2, 3>::value << '\n';
  std::cout << "9 < 1    = " << lt<9, 1>::value << '\n';
  std::cout << "5 > 3    = " << gt<5, 3>::value << '\n';
  std::cout << "8 > 12   = " << gt<8, 12>::value << '\n';
  std::cout << "5 = 5    = " << eq<5, 5>::value << '\n';
  std::cout << "3 = 2    = " << eq<3, 2>::value << '\n';
  std::cout << "8 =/= 9  = " << neq<8, 9>::value << '\n';
  std::cout << "5 =/= 5  = " << neq<5, 5>::value << '\n';
  std::cout << "7^2      = " << square<7>::value << '\n';
  std::cout << "0^2      = " << square<0>::value << '\n';
  std::cout << "sqrt(0)  = " << sqrt<0>::value << '\n';
  std::cout << "sqrt(1)  = " << sqrt<1>::value << '\n';
  std::cout << "sqrt(25) = " << sqrt<25>::value << '\n';
  // std::cout << "sqrt(6)  = " << sqrt<6>::value << '\n';  // Undefined!
}

