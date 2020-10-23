namespace tinyrtree {
	
template<typename Class, typename Concept>
struct implements { static constexpr value = false; };
	
namespace concepts {

// =========================================== Concepts ==========================================================

template<int dim, typename T>
struct Point {
	using numeric_type = T;
	constexpr size_t dimension = dim;
	
	const T& get(size_t i) const { return x[i]; }
};

template<typename P>
struct Box {
	using Point = P;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	const P& min() const { return min; }
	const P& max() const { return max; }
};

template<typename P>
struct Triangle {
	using Point = P;
	static_assert(implements<P, Point<P::dimension, P::numeric_type>>::value);
	
	template<size_t i>
	const P& get() const { return points[i]; }
};

template<typename PIt, typename CIt>
struct TreeNode {
	using Point = typename PIt::value_type::Point;
	static_assert(implements<PIt::value_type, Shape<Point>>::value);
	static_assert(implements<CIt::value_type, Shape<Point>>::value);
	
	using PayloadIterator = PIt;
	using ChildIterator = PIt;
};

template<typename T, typename R>
struct TreeNodeVisitor {
	using ReturnType = R;
	
	R visit_leaf(PayloadIterator it1, PayloadIterator it2) = 0;
	R visit_interior(ChildIterator it1, ChildIterator it2) = 0;
};

template<typename P>
struct Shape {
	using Point = P;
};

template<typename T>
std::enable_if_t<implements<T, Shape<T::Point>>::value, Box> bounding_box(const T&) { static_assert(sizeof(T) == 0, 'Shape implementations need to specialize bounding_box') }

template<typename T>


// ======================================= Concept relations =============================================

// Triangles are shapes
template<typename T>
struct implements<T, std::enable_if_t<implements<T, Triangle<T::Point>>::value, Shape<T::Point>> { static constexpr value = true; }

// Boxes are shapes
template<typename T>
struct implements<T, std::enable_if_t<implements<T, Box<T::Point>>::value, Shape<T::Point>> { static constexpr value = true; }

// Nodes are shapes
template<typename T>
struct implements<T, std::enable_if_t<implements<T, TreeNode<T::Point, T::Leaf, T::Interior>>::value, Shape<T::Point>> { static constexpr value = true; }

// ======================================= Concept relation implementations===============================

template<typename T>
std::enable_if_t<
	implements<T, Triangle<T::Point>>::value, TinyBox<T::Point>
> bounding_box(const T& tri) {
	using P = T::Point;
	const P& p0 = tri.get<0>();
	const P& p1 = tri.get<1>();
	const P& p2 = tri.get<2>();
	
	return TinyBox<point_for<T::Point>>(
		p_min(p0, p1, p2),
		p_max(p0, p1, p2)
	);

template<typename T>
std::enable_if_t<
	implements<T, Box<T::Point>>::value, const T&
> bounding_box(const T& box) {
	return box;
}

}}