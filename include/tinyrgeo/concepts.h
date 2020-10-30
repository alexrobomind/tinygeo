namespace tinyrgeo {

namespace tags {
	enum tag {
		box,
		triangle,
		node
	};
}
	
namespace concepts {

// =========================================== Concepts ==========================================================

template<typename P>
struct Shape {
	Box<P> bounding_box() { throw std::exception("Not implemented"); }
}

template<int dim, typename T>
struct Point {
	static constexpr tag = tags::point;
	
	using numeric_type = T;
	constexpr size_t dimension = dim;
	
	T operator[](size_t i) const { return x[i]; }
};

template<typename P>
struct Box : public Shape<P> {
	static constexpr tag = tags::box;
	
	using Point = P;
	
	const P& min() const = 0;
	const P& max() const = 0;
	
	Box<P> bounding_box() const = 0;
};

template<typename P>
struct Triangle : public Shape<P>{
	static constexpr tag = tags::triangle;
	
	using Point = P;
	
	const P& operator[](size_t i) const = 0;
	
	Box<P> bounding_box() const = 0;
};

template<typename PIt, typename CIt>
struct Node : public Shape<PIt::value_type::Point>{
	static constexpr tag = tags::node;
	
	using Point = typename PIt::value_type::Point;
	
	using DataIterator  = PIt;
	using ChildIterator = CIt;
	
	DataIterator begin_data() = 0;
	DataIterator end_data() = 0;
	ChildIterator begin_children() = 0;
	ChildIterator end_children() = 0;
	
	Box<Point> bounding_box() const = 0;
};

template<typename T>
struct Buffer {
	Buffer(const Buffer<T>& other) {}
	Buffer<T>& operator=(const Buffer<T>& other) = 0:
	
	const T& operator(size_t i, size_t j) const = 0;
	void set(size_t i, size_t j, const T& val) = 0;
}

}}