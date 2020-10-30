#include <tinyrgeo/pack.h>

namespace tinyrgeo {

template<size_t dim, typename PointBuffer, typename IndexBuffer>
struct TriangleMesh {
	struct Accessor {
		TriangleMesh& parent;
		size_t index;
		
		Accessor(TriangleMesh& parent, size_t index) : parent(parent), index(index) {}
		
		struct Point {
			using numeric_type = typename PointBuffer::Type;
			static constexpr size_t dimension = dim;
			
			TriangleMesh& parent;
			size_t index;
			
			Point(TriangleMesh& parent, size_t index) : parent(parent), index(index) {}
	
			numeric_type& operator[](size_t i) {
				return parent.point_buffer(index, i);
			}
		};
		
		template<size_t idx>
		Point get() const {
			return Point(parent, parent.index_buffer(index, idx));
		}
		
		Point operator[](size_t idx) const {
			return Point(parent, parent.index_buffer(index, idx));
		}
	};
	
	struct Iterator {
		Accessor acc;
		
		Iterator(TriangleMesh& mesh, size_t index) :
			acc(mesh, index)
		{}
		
		Iterator& operator++() { ++acc.index; return *this;	}
		bool operator==(Iterator& other) { return acc.index == other.acc.index; }
		bool operator!=(Iterator& other) { return acc.index != other.acc.index; }
		
		Accessor& operator*() { return acc; }
	};
	
	using Point = typename Accessor::Point;
	
	TriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer) :
		point_buffer(point_buffer),
		index_buffer(index_buffer)
	{}
		
	PointBuffer point_buffer;
	IndexBuffer index_buffer;
	
	size_t size() {
		return index_buffer.size() / 3;
	}
	
	Iterator begin() { return Iterator(*this, 0); }
	Iterator end() { return Iterator(*this, size()); }
	
	Accessor operator[](size_t i) {
		return Accessor(*this, i);
	}
}

template<size_t dim, typename PointBuffer, typename IndexBuffer, typename NodeData>
struct IndexedTriangleMesh : public TriangleMesh<dim, PointBuffer, IndexBuffer> {
	using Parent = TriangleMesh<dim, PointBuffer, IndexBuffer>;
	
	using typename Parent::Point;
	using typename Parent::Accessor;
	using typename Parent::Iterator;
	
	struct Node {
		TriangleMesh& mesh;
		NodeData data;
		
		using LeafIterator = Iterator;
		struct ChildIterator {
			mutable Node tmp;
			NodeData::ChildIterator dit;
			
			NodeIterator(TriangleMesh& mesh, const It& it) :
				tmp(mesh, *it),
				dit(it)
			{}
			
			Node& operator*() const { tmp.data = *dit; return tmp; }
			
			bool operator==(const NodeIterator<DataIt>& other) const { return dit == other.dit; }
			NodeIterator& operator++() { ++dit; return *this; }
		};
		
		LeafIterator begin_data() { return Iterator(mesh, data.leaf_range().first); }
		LeafIterator end_data() { return Iterator(mesh, data.leaf_range().second); }
		ChildIterator begin_children() { return ChildIterator(mesh, data.begin_children()); }
		ChildIterator end_children() { return ChildIterator(mesh, data.end_children()); }
		
		auto& bounding_box() { return data.bounding_box(); }
	};
	
	TriangleMesh(const PointBuffer& point_buffer, const IndexBuffer& index_buffer, const NodeData& root_data) :
		Parent(point_buffer, index_buffer),
		root_data(root_data)
	{}
	
	NodeData root_data;
	
	Node root() {
		return Node(*this, root_data);
	}
	
	void pack() {
		auto r = root();
		
		using PackNode = tinyrgeo::Node<Accessor>;
		PackNode pack_result = tinyrgeo::pack(r.begin_data(), r.end_data());
		
		IndexBuffer new_buffer(this -> index_buffer.size(), 3);
		size_t counter = 0;
		
		std::list<std::pair<NodeData&, PackNode> queue;
		
		auto process = [&](Node n) {
			size_t count = node.data.size();
			
			NodeData out(counter, counter + count, node.children.size());
			for(size_t i = 0; i < count; ++i) {
				for(size_t j = 0; j < 3; ++j) {
					new_buffer.set(
						counter + i, j,
						index_buffer.get(node.data[i].index, j)
					);
				}
			}
			
			counter += count;
			return out;
		}
		
		root_data = process(pack_result);
		queue.push_back(std::make_pair(root_data, pack_result));
		
		for(auto it = queue.begin(); it != queue.end(); ++it) {
			NodeData& out = it->first;
			PackNode& in  = it->second;
			
			for(size_t i = 0; i < in.children.size(); ++i) {
				out.child(i) = process(in.children[i]);
				queue.push_back(std::make_pair(out.get_child[i], )
			}
		}
	}
};

struct SimpleData {
	using ChildHolder = std::list<SimpleData>;
	using ChildIterator = ChildHolder::iterator;
	
	size_t start;
	size_t end;
	ChildHolder children;
	
	std::pair<size_t, size_t> leaf_range() { return std::make_pair(start, end); }
	
	ChildIterator begin_children() { return children.begin(); }
	ChildIterator end_children() { return children.end(); }
	SimpleData& child(size_t i) { return children[i]; }
	
	SimpleData(size_t start, size_t end, size_t ndata) :
		start(start), end(end), children(ndata)
	{}
};

}