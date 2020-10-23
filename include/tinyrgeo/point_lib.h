namespace tinyrtree {
	
template<typename T>
using point_for = Point<PointInfo<T>::dimension, PointInfo<T>::numeric_type>;

template<typename T...>
auto p_min(const T& t...) {
	return internal:::p_elmap(internal::allmin, t...)
}

template<typename T...>
auto p_max(const T& t...) {
	return internal:::p_elmap(internal::allmax, t...)
}

template<typename P1, typename P2>
point_for<P1> half_point(const P1& p1, const P2& p2) {
	num = P1::numeric_type;
	
	auto l = [](num n1, num n2) { return 0.5 * (n1 + n2); }
	return p_elmap(l, p1, p2);
}

namespace internal {
	template<typename F>
	auto p_elmap(F f) {
		return [f](auto p, const auto&... rem) {
			using P = decltype<p>;
			
			constexpr size_t dim = PointInfo<P>::dimension;
			
			point_for<P> result;
			for(size_t i = 0; i < dim; ++i)
				result[i] = f(p.get(i), rem.get(i)...);
			
			return result;
		};
	}
	
	template<typename T...>
	auto allmin(const T& t...) { return std::min({t...}); }
	
	template<typename T...>
	auto allmax(const T& t...) { return std::max({t...}); }
}

template<typename T>
void assign(point_for<T>& dst, const T& src) {
	for(size_t i = 0; i < T::dimension; ++i)
		dst.get(i) = src.get(i)
}

}