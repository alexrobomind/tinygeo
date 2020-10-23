namespace tinyrtree {

template<int dim, typename T>
struct Point {
	using numeric_type = T;
	constexpr size_t dimension = dim;
	
	T x[dim];
	
	Point() {
		for(size_t i = 0; i < dim; ++i)
			x[i] = 0;
	}
	
	const T& get(size_t i) const { return x[i]; }
	      T& get(size_t i)       { return x[i]; }
};

template<typename P>
struct Box {
	using Point = P;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	P min;
	P max;
	
	const P& min() const { return min; }
	const P& max() const { return max; }
	
	      P& min()       { return min; }
	      P& max()       { return max; }
	
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
struct Triangle {
	using Point = point_for<P>;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	P points[3];
	
	template<size_t i>
	const P& get() const { return points[i]; }
	
	template<size_t i>
	      P& get()       { return points[i]; }
};

template<int dim, typename T>
struct implements<Point<dim, t>, concepts::Point<dim, t>> { static constexpr value = true; }

template<typename P>
struct implements<Box<P>, concepts::Box<P>> { static constexpr value = true; }

template<typename P>
struct implements<Triangle<P>, concepts::Triangle<P>> { static constexpr value = true; }

}