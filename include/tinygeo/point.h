#pragma once

#include <tinygeo/concepts.h>

namespace tinygeo {

template<size_t dim, typename T>
struct Point {
	using numeric_type = T;
	constexpr static size_t dimension = dim;
	
	T x[dim];
	
	Point() = default;
	
	template<typename P>
	Point(const P& other) {
		for(size_t i = 0; i < dim; ++i)
			x[i] = other[i];
	}
	
	Point(const std::array<T, dim>& input) {
		for(size_t i = 0; i < dim; ++i)
			x[i] = input[i];
	}
	
	Point(const std::initializer_list<T>& input) :
		Point(std::array<T, dim>(input))
	{}
	
	Point(const std::valarray<T>& input) {
		for(size_t i = 0; i < dim; ++i)
			x[i] = input[i];
	}
	
	const T& operator[](size_t i) const { return x[i]; }
	      T& operator[](size_t i)       { return x[i]; }
};
	
template<typename T>
using point_for = Point<T::dimension, typename T::numeric_type>;

namespace internal {
	template<typename F>
	auto p_elmap(F f) {
		return [f](auto p, const auto&... rem) {
			using P = decltype(p);
			
			constexpr size_t dim = P::dimension;
			
			point_for<P> result;
			for(size_t i = 0; i < dim; ++i)
				result[i] = f(p[i], rem[i]...);
			
			return result;
		};
	}
	
	template<typename... T>
	struct funcs {
		static auto allmin(const T&... t) { return std::min({t...}); }
		static auto allmax(const T&... t) { return std::max({t...}); }
	};
}

template<typename... T>
auto p_min(const T&... t) {
	return internal::p_elmap(internal::funcs<typename T::numeric_type...>::allmin)(t...);
}

template<typename... T>
auto p_max(const T&... t) {
	return internal::p_elmap(internal::funcs<typename T::numeric_type...>::allmax)(t...);
}

template<typename P1, typename P2>
point_for<P1> half_point(const P1& p1, const P2& p2) {
	using num = typename P1::numeric_type;
	
	auto l = [](num n1, num n2) { return 0.5 * (n1 + n2); };
	return internal::p_elmap(l)(p1, p2);
}

template<typename T>
void assign(point_for<T>& dst, const T& src) {
	for(size_t i = 0; i < T::dimension; ++i)
		dst[i] = src[i];
}

}