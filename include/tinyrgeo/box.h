namespace tinyrgeo {

template<typename P>
struct Box {
	using Point = P;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	P min;
	P max;
	
	Box(const P& min, const P& max) : min(min), max(max) {}
	
	const P& min() const { return min; }
	const P& max() const { return max; }
	
	      P& min()       { return min; }
	      P& max()       { return max; }
	
	const Box<P>& bounding_box() const { return *this; }
	
	static Box<point_for<P>> empty() {
		Box<point_for<P>> result;
		using Num = P::numeric_type;
		
		Num inf = std::numeric_limits<Num>::infinity();
		for(size_t i = 0; i < dim; ++i) {
			min.x[i] = inf;
			max.x[i] = -inf;
		}
		
		return result;
	}
	
	static Box<point_for<P>> empty() {
		Box<point_for<P>> result;
		using Num = P::numeric_type;
		
		Num inf = std::numeric_limits<Num>::infinity();
		for(size_t i = 0; i < dim; ++i) {
			min.x[i] = -inf;
			max.x[i] = inf;
		}
	}
	
	return result;
};

template<typename P>
struct implements<Box<P>, tags::box> { static constexpr value = true; }

template<typename B1, typename... Rem>
static Box<point_for<B1::Point>> combine_boxes(const B1& box1, const Rem&... rem) {
	// Unfortuantely I have no way of doing this with the other boxes as well
	static_assert(is_box<B1>::value)
	
	return Box<point_for<B1::Point>>(
		p_min(box1.get_min(), rem.get_min()...),
		p_max(box1.get_min(), rem.get_min()...)
	);
}

}