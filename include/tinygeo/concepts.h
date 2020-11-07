#pragma once

namespace tinygeo {

namespace tags {
	enum tag {
		box,
		triangle,
		node
	};
}
	
namespace concepts {
	
using std::size_t;
	
template<typename T, tags::tag t>
struct implements { constexpr static bool value = false; };

// =========================================== Concepts ==========================================================

template<int dim, typename T>
struct Point {	
	using numeric_type = T;
	static constexpr size_t dimension = dim;
	
	T operator[](size_t i) const { throw std::exception("Not implemented"); }
};

template<typename P> struct Box;

template<typename P>
struct Shape {
	using Point = P;
	Box<P> bounding_box() { throw std::exception("Not implemented"); }
};

template<typename P>
struct Box : public Shape<P> {
	static constexpr tags::tag tag = tags::box;
	
	const P& min() const { throw std::exception("Not implemented"); }
	const P& max() const { throw std::exception("Not implemented"); }
	
	using Point = P;
	Box<P> bounding_box() const { throw std::exception("Not implemented"); }
};

template<typename P>
struct Triangle : public Shape<P>{
	static constexpr tags::tag tag = tags::triangle;
	
	const P& operator[](size_t i) const { throw std::exception("Not implemented"); }
	
	using Point = P;
	Box<P> bounding_box() const { throw std::exception("Not implemented"); }
};

template<typename D>
struct Node : public Shape<typename D::Point>{
	static constexpr tags::tag tag = tags::node;
	
	size_t n_children() const { throw std::exception("Not implemented"); }
	Node<D> child(size_t i) const { throw std::exception("Not implemented"); }
	
	size_t n_data() const { throw std::exception("Not implemented"); }
	D data(size_t i) const { throw std::exception("Not implemented"); }
	
	using Point = typename D::Point;
	Box<Point> bounding_box() const { throw std::exception("Not implemented"); }
};

template<typename T>
struct Buffer {
	Buffer(const Buffer<T>& other) { throw std::exception("Not implemented"); }
	Buffer<T>& operator=(const Buffer<T>& other) { throw std::exception("Not implemented"); }
	
	const T& operator()(size_t i, size_t j) const { throw std::exception("Not implemented"); }
	void set(size_t i, size_t j, const T& val) { throw std::exception("Not implemented"); }
};

}}