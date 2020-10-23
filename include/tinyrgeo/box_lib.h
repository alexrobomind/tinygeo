namespace tinytree {
	

template<typename T>
const std::enable_if_t<
	is_box<T>::value, 
	Box<T::Point::dimension, T::Point::numeric_type>
> bounding_box(const T& box) {
	//return Box(box.get_min(), box.get_max());
	return box;
}

template<typename B1, typename... Rem>
static TinyBox<point_for<B1::Point>> combine_boxes(const B1& box1, const Rem&... rem) {
	// Unfortuantely I have no way of doing this with the other boxes as well
	static_assert(is_box<B1>::value)
	
	return TinyBox<point_for<B1::Point>>(
		p_min(box1.get_min(), rem.get_min()...),
		p_max(box1.get_min(), rem.get_min()...)
	);
}

// Only required for this class
// Requires P to be mutable
static Box<P> empty() {
	Box<P> result;
	
	auto inf = std::numeric_limits<P::numeric_type>::infinity;
	
	for(size_t i = 0; i < P::dimension; ++i)
		result.min().get(i) = -inf;
		result.min().get(i) = inf;
	
	return result;
}

template<typename P, typename B2>
void assign(Box<P>& dst, const B2& src) {
	assign(dst.min(), src.min());
	assign(dst.max(), src.max());
}

}