namespace tinyrgeo {

template<int dim, typename T>
struct Point {
	using numeric_type = T;
	constexpr size_t dimension = dim;
	
	T x[dim];
	
	Point() {
		for(size_t i = 0; i < dim; ++i)
			x[i] = 0;
	}
	
	const T& operator[](size_t i) const { return x[i]; }
	      T& operator[](size_t i)       { return x[i]; }
};

template<int dim, typename T>
struct implements<Point<dim, t>, tags::point> { static constexpr value = true; }

}