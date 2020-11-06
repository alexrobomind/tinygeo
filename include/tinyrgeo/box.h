#pragma once

#include <tinyrgeo/concepts.h>
#include <tinyrgeo/point.h>

namespace tinyrgeo {

template<typename P>
struct Box {
	constexpr static tags::tag tag = tags::box;
	
	using Point = P;
	
	Box(const P& min, const P& max) : pmin(min), pmax(max) {}
	Box() {}
	
	const P& min() const { return pmin; }
	const P& max() const { return pmax; }
	
	      P& min()       { return pmin; }
	      P& max()       { return pmax; }
	
	const Box<P>& bounding_box() const { return *this; }
	
	static Box<point_for<P>> empty() {
		Box<point_for<P>> result;
		using Num = typename P::numeric_type;
		
		Num inf = std::numeric_limits<Num>::infinity();
		for(size_t i = 0; i < P::dimension; ++i) {
			result.min()[i] = inf;
			result.max()[i] = -inf;
		}
		
		return result;
	}
	
private:
	P pmin;
	P pmax;
};

namespace concepts {
	template<typename P>
	struct implements<::tinyrgeo::Box<P>, tags::box> { static constexpr bool value = true; };
}

template<typename B1, typename... Rem>
static Box<point_for<typename B1::Point>> combine_boxes(const B1& box1, const Rem&... rem) {	
	return Box<point_for<typename B1::Point>>(
		p_min(box1.min(), rem.min()...),
		p_max(box1.max(), rem.max()...)
	);
}

template<typename B>
point_for<typename B::Point> center(const B& b) {
	using RT = std::enable_if_t<concepts::implements<B, tags::box>::value, point_for<typename B::Point>>;
	
	return half_point(b.min(), b.max());
}

template<typename B>
bool is_empty(const B& b) {
	for(size_t i = 0; i < B::Point::dimension; ++i) {
		if(b.max()[i] < b.min()[i])
			return true;
	}
	return false;
}

}