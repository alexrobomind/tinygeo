namespace tinyrgeo {

template<typename T>
static Box<point_for<T::Point>> triangle_bounding_box(const T& tri) {
	using P = point_for<T::Point>;
	
	const P p0 = tri.get<0>();
	const P p1 = tri.get<1>();
	const P p2 = tri.get<2>();
	
	return Box<P>(
		p_min(p0, p1, p2),
		p_max(p0, p1, p2)
	);
}

template<typename P>
struct Triangle {
	using Point = point_for<P>;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	P points[3];
	
	template<size_t i>
	const P& get() const { return points[i]; }
	
	template<size_t i>
	P& get()       { return points[i]; }
		  
	auto bounding_box() const { return triangle_bounding_box(*this); }
};

template<typename P>
struct implements<Triangle<P>, tags::triangle> { static constexpr value = true; }

}