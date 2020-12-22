#pragma once

#include <list>

#include <tinygeo/pack.h>
#include <tinygeo/triangle.h>

namespace tinygeo {

// A triangle mesh that stores its data inside a vertex and index buffer
template<size_t dim, typename PointBuffer, typename IndexBuffer, typename TagBuffer>
struct TriangleMesh {
	struct Accessor {
		static constexpr tags::tag tag = tags::triangle;
		using tag_type = typename TagBuffer::Type;
		
		TriangleMesh* parent;
		size_t index;
		
		Accessor(TriangleMesh* parent, size_t index) : parent(parent), index(index) {}
		
		struct Point {
			using numeric_type = typename PointBuffer::Type;
			static constexpr size_t dimension = dim;
			
			TriangleMesh& parent;
			size_t index;
			
			Point(TriangleMesh& parent, size_t index) : parent(parent), index(index) {}
	
			typename PointBuffer::Ref operator[](size_t i) {
				return parent.point_buffer(index, i);
			}
			
			const typename PointBuffer::Ref operator[](size_t i) const {
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
		
		std::vector<tag_type> tags() const {
			const size_t n_tags = parent -> tag_buffer.shape(1);
			
			std::vector<tag_type> result(n_tags);
			for(size_t i = 0; i < n_tags; ++i)
				result[i] = parent -> tag_buffer(index, i);
			
			return result;
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
		
		Iterator operator+(size_t di) {	return Iterator(*acc.parent, acc.index + di); }
	};
	
	using Point = typename Accessor::Point;
	
	TriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer, const TagBuffer& tag_buffer) :
		point_buffer(point_buffer),
		index_buffer(index_buffer),
		tag_buffer(tag_buffer)
	{}
		
	PointBuffer point_buffer;
	IndexBuffer index_buffer;
	TagBuffer tag_buffer;
	
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
template<size_t dim, typename PointBuffer, typename IndexBuffer, typename TagBuffer, typename NodeData, typename GridData>
struct IndexedTriangleMesh : public TriangleMesh<dim, PointBuffer, IndexBuffer, TagBuffer> {
	using Parent = TriangleMesh<dim, PointBuffer, IndexBuffer, TagBuffer>;
	using Self = IndexedTriangleMesh<dim, PointBuffer, IndexBuffer, TagBuffer, NodeData, GridData>;
	
	using typename Parent::Point;
	using typename Parent::Accessor;
	using typename Parent::Iterator;
	
	static constexpr size_t dimension = Point::dimension;
	
	struct Node {
		using Point = typename Parent::Point;
		static constexpr tags::tag tag = tags::node;
		using tag_type = typename TagBuffer::Type;
		
		Self& mesh;
		const NodeData& rdata;
		
		Node(Self& mesh, const NodeData& data) : mesh(mesh), rdata(data) {}
		
		size_t n_children() const { return rdata.n_children(); }
		Node child(size_t i) const { return Node(mesh, rdata.child(i)); }
		size_t n_data() const { return rdata.range().second - rdata.range().first; }
		Accessor data(size_t i) const { return Accessor(&mesh, rdata.range().first + i); }
		
		auto bounding_box() { return rdata.bounding_box(); }
	};
	
	struct Grid {
		using Point = typename Parent::Point;
		static constexpr tags::tag tag = tags::grid;
		using tag_type = typename TagBuffer::Type;
	
		using MultiIndex = std::array<std::size_t, dimension>;
				
		Self& mesh;
		
		MultiIndex size;
	
		GridData data;
		
		Grid(Self& mesh, const GridData& data) :
			mesh(mesh), data(data)
		{
			for(size_t d = 0; d < dimension; ++d)
				size[d] = 1;
		}
		
		auto bounding_box() const { return mesh.root().bounding_box(); }
		
		MultiIndex index_for(const point_for<Point>& p) const {
			using Num = typename Point::numeric_type;
			
			auto bb = bounding_box();
			
			point_for<Point> min = bb.min();
			point_for<Point> max = bb.max();
			
			MultiIndex result;
			
			for(size_t i = 0; i < dimension; ++i) {
				Num fp = (p[i] - min[i]) / (max[i] - min[i]);
				fp *= size[i];
				
				if(fp < 0)
					fp = 0;
				
				std::size_t val = (std::size_t) fp;
				if(val >= size[i])
					val = size[i] - 1;
			
				result[i] = val;
			}
			
			return result;
		}
		
		std::list<Accessor> query(const MultiIndex i1, const MultiIndex i2) const {			
			MultiIndex low;
			MultiIndex high;
			
			for(size_t d = 0; d < dimension; ++d) {
				low[d] = std::min(i1[d], i2[d]);
				high[d] = std::max(i1[d], i2[d]);
			}
			
			std::list<Accessor> result;
			
			MultiIndex c = low;
			do {
				//pybind11::print("Checking position ", c);
				
				for(size_t idx : data.get(linear_index(c)))
					result.push_back(Accessor(&mesh, idx));
			} while(increment(c, low, high));
			
			return result;
		}
	
		void pack() {			
			// Resize data structure
			data.reset(linear_size());
			
			// Distribute triangles into ranges they intersect
			for(Accessor acc : mesh) {
				point_for<Point> p1 = acc.bounding_box().min();
				point_for<Point> p2 = acc.bounding_box().max();
				
				MultiIndex i1 = index_for(p1);
				MultiIndex i2 = index_for(p2);
				
				MultiIndex low;
				MultiIndex high;
				for(size_t d = 0; d < dimension; ++d) {
					low[d] = std::min(i1[d], i2[d]);
					high[d] = std::max(i1[d], i2[d]);
				}
				
				MultiIndex c = low;
				do {
					data.insert(linear_index(c), acc.index);
				} while(increment(c, low, high));
			}
		}
	
	private:
		size_t linear_index(const MultiIndex i) const {
			size_t result = 0;
			
			for(size_t d = 0; d < dimension; ++d) {
				result *= size[d];
				result += i[d];
			}
			
			return result;
		}
		
		bool increment(MultiIndex& i, const MultiIndex low, const MultiIndex high) const {
			for(size_t d = 0; d < dimension; ++d) {
				if(i[d] < high[d]) {
					++i[d];
					return true;
				}
				
				i[d] = low[d];
			}
			
			return false;
		}
		
		size_t linear_size() const {
			size_t result = 1;
			
			for(auto s : this -> size)
				result *= s;
			
			return result;
		}
	};
	
	IndexedTriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer, const TagBuffer& tag_buffer, const NodeData& root_data, const GridData& grid_data) :
		Parent(point_buffer, index_buffer, tag_buffer),
		root_data(root_data),
		grid(*this, grid_data)
	{
		if(index_buffer.shape(0) != tag_buffer.shape(0))
			throw std::invalid_argument("Index and tag buffer must have identical first dimension");
	}
	
	NodeData root_data;
	Grid grid;
	
	Node root() {
		return Node(*this, root_data);
	}
	
	void pack(size_t size) {
		// Pack up the data contained in this node
		Node r = root();
		
		using PackNode = tinygeo::PackNode<Accessor>;
		PackNode pack_result = tinygeo::pack(this -> begin(), this -> end(), size);
		
		// Allocate new index and tag buffer
		IndexBuffer new_buffer    (this -> index_buffer.shape(0), 3);
		TagBuffer   new_tag_buffer(this -> tag_buffer.shape(0)  , this -> tag_buffer.shape(1));
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
				
				for(size_t j = 0; j < this -> tag_buffer.shape(1); ++j) {
					new_tag_buffer(counter + i, j) = this -> tag_buffer(in.data[i].index, j);
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
		this -> tag_buffer   = new_tag_buffer;
		
		grid.pack();
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

struct SimpleGridData {
	std::vector<std::list<size_t>> data;
	
	const std::list<size_t>& get(size_t i) const { 
		if(data.size() == 0)
			throw std::logic_error("query called on unitialized grid data");
		
		return data[i];
	}
	
	size_t size() const {
		return data.size();
	}
	
	void reset(size_t n) {
		data.clear();
		data.resize(n);
	}
	
	void insert(size_t i, size_t val) {
		data[i].push_back(val);
	}
};

}
