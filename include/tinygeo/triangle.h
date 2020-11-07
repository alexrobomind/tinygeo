#pragma once

#include <tinygeo/concepts.h>

namespace tinygeo {

template<typename T>
static Box<point_for<typename T::Point>> triangle_bounding_box(const T& tri) {
	using P = point_for<typename T::Point>;
	
	const P p0 = tri.template get<0>();
	const P p1 = tri.template get<1>();
	const P p2 = tri.template get<2>();
	
	return Box<P>(
		p_min(p0, p1, p2),
		p_max(p0, p1, p2)
	);
}

template<typename P>
struct Triangle {
	using Point = point_for<P>;
	
	P points[3];
	
	template<size_t i>
	const P& get() const { return points[i]; }
	
	template<size_t i>
	P& get()       { return points[i]; }
		  
	auto bounding_box() const { return triangle_bounding_box(*this); }
};

namespace concepts {
	template<typename P>
	struct implements<Triangle<P>, tags::triangle> { static constexpr bool value = true; };
}

}