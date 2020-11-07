#pragma once

#include <list>

#include <tinygeo/pack.h>
#include <tinygeo/triangle.h>

namespace tinygeo {

// A triangle mesh that stores its data inside a vertex and index buffer
template<size_t dim, typename PointBuffer, typename IndexBuffer>
struct TriangleMesh {
	struct Accessor {
		static constexpr tags::tag tag = tags::triangle;
		
		TriangleMesh* parent;
		size_t index;
		
		Accessor(TriangleMesh* parent, size_t index) : parent(parent), index(index) {}
		
		struct Point {
			using numeric_type = typename PointBuffer::Type;
			static constexpr size_t dimension = dim;
			
			TriangleMesh& parent;
			size_t index;
			
			Point(TriangleMesh& parent, size_t index) : parent(parent), index(index) {}
	
			numeric_type& operator[](size_t i) {
				return parent.point_buffer(index, i);
			}
			
			const numeric_type& operator[](size_t i) const {
				return parent.point_buffer(index, i);
			}
		};
		
		template<size_t idx>
		Point get() const {
			return Point(*parent, parent->index_buffer(index, idx));
		}
		
		Point operator[](size_t idx) const {
			return Point(*parent, parent->index_buffer(index, idx));
		}
		
		Box<point_for<Point>> bounding_box() const {
			return triangle_bounding_box(*this);
		}
	};
	
	struct Iterator {
		using value_type = Accessor;
		using reference = Accessor&;
		using pointer = Accessor*;
		using difference_type = size_t;
		using iterator_category = std::input_iterator_tag;
		
		Accessor acc;
		
		Iterator(TriangleMesh& mesh, size_t index) :
			acc(&mesh, index)
		{}
		
		Iterator& operator++() { ++acc.index; return *this;	}
		bool operator==(const Iterator& other) { return acc.index == other.acc.index; }
		bool operator!=(const Iterator& other) { return acc.index != other.acc.index; }
		
		Accessor& operator*() { return acc; }
		Accessor* operator->() { return &acc; }
	};
	
	using Point = typename Accessor::Point;
	
	TriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer) :
		point_buffer(point_buffer),
		index_buffer(index_buffer)
	{}
		
	PointBuffer point_buffer;
	IndexBuffer index_buffer;
	
	size_t size() {
		return index_buffer.shape(0);
	}
	
	Iterator begin() { return Iterator(*this, 0); }
	Iterator end() { return Iterator(*this, size()); }
	
	Accessor operator[](size_t i) {
		return Accessor(this, i);
	}
};

// An extension of the TriangleMesh template that includes indexing by an R-Tree
template<size_t dim, typename PointBuffer, typename IndexBuffer, typename NodeData>
struct IndexedTriangleMesh : public TriangleMesh<dim, PointBuffer, IndexBuffer> {
	using Parent = TriangleMesh<dim, PointBuffer, IndexBuffer>;
	using Self = IndexedTriangleMesh<dim, PointBuffer, IndexBuffer, NodeData>;
	
	using typename Parent::Point;
	using typename Parent::Accessor;
	using typename Parent::Iterator;
	
	struct Node {
		using Point = typename Parent::Point;
		static constexpr tags::tag tag = tags::node;
		
		Self& mesh;
		const NodeData& rdata;
		
		Node(Self& mesh, const NodeData& data) : mesh(mesh), rdata(data) {}
		
		size_t n_children() const { return rdata.n_children(); }
		Node child(size_t i) const { return Node(mesh, rdata.child(i)); }
		size_t n_data() const { return rdata.range().second - rdata.range().first; }
		Accessor data(size_t i) const { return Accessor(&mesh, rdata.range().first + i); }
		
		auto& bounding_box() { return rdata.bounding_box(); }
	};
	
	IndexedTriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer, const NodeData& root_data) :
		Parent(point_buffer, index_buffer),
		root_data(root_data)
	{}
	
	NodeData root_data;
	
	Node root() {
		return Node(*this, root_data);
	}
	
	void pack(size_t size) {
		// Pack up the data contained in this node
		Node r = root();
		
		using PackNode = tinygeo::PackNode<Accessor>;
		PackNode pack_result = tinygeo::pack(this -> begin(), this -> end(), size);
		
		// Allocate new index buffer
		IndexBuffer new_buffer(this -> index_buffer.shape(0), 3);
		size_t counter = 0;
		
		// Insert all nodes into the queue
		std::list<std::pair<PackNode, std::reference_wrapper<NodeData>>> queue;
				
		auto process = [&,this](const PackNode& in, NodeData& out) {
			const size_t count = in.data.size();
			
			// Set allocated range in node data
			out.set_start(counter);
			out.set_end(counter + count);
			out.bounding_box() = in.box;
			
			// Copy indices into allocated range
			for(size_t i = 0; i < count; ++i) {
				for(size_t j = 0; j < 3; ++j) {
					new_buffer(counter + i, j) = this -> index_buffer(in.data[i].index, j);
				}
			}
			counter += count;
			
			// Add children to queue
			const size_t n_c = in.children.size();
			out.init_children(n_c);
			for(size_t i = 0; i < n_c; ++i)
				queue.push_back(std::make_pair(in.children[i], std::ref(out.child(i))));
		};
		
		queue.push_back(std::make_pair(pack_result, std::ref(root_data)));
		for(auto it = queue.begin(); it != queue.end(); ++it) {
			const PackNode& in = it -> first;
			NodeData& out = it -> second;
			process(in, out);
		}
		
		this -> index_buffer = new_buffer;
	}
};

template<typename P>
struct SimpleNodeData {
	std::pair<size_t, size_t> range() const { return std::make_pair(start, end); }
	void set_start(size_t val) { start = val; }
	void set_end(size_t val) { end = val; }
	
	void init_children(size_t s) { children.resize(s); }
	size_t n_children() const { return children.size(); }
	SimpleNodeData& child(size_t i) { return children[i]; }
	const SimpleNodeData& child(size_t i) const { return children[i]; }
	
	const Box<P>& bounding_box() const { return bb; }
	Box<P>& bounding_box() { return bb; }
	
	SimpleNodeData() : start(0), end(0), children(0), bb(Box<P>::empty()) {}
	
private:
	using ChildHolder = std::vector<SimpleNodeData<P>>;
	using ChildIterator = typename ChildHolder::iterator;
	
	size_t start;
	size_t end;
	ChildHolder children;
	Box<P> bb;
};

}