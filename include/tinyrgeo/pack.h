namespace tinyrtree {
	
	// An indirection required to build and implement the treenode implementation below.
	
	template<typename T>
	struct ShapeRef<T> {
		using Point = typename T::Point;
		using Target = T;
		
		std::shared_ptr<T> target;
	};
	
	template<typename T>
	struct implements<ShapeRef<T>, concepts::Shape<T::Point>> { static constexpr value = true; }
	
	template<typename T>
	auto bounding_box(const ShapeRef<T>& in) { return bounding_box(*(in.target)); }
	
	/** An iterator for iterating over the targets referenced by shape ref lists*/
	template<typename It>
	struct ShapeRefListIterator {
		using Self = ShapeRefListIterator<It>;
		using Ref = It::value_type;
		using Target = Ref::Target;
		
		using value_type = const Target;
		using pointer = const Target*;
		using reference = const Target&;
		using iterator_category = std::input_iterator_tag;
		using difference_type = It::difference_type;
		
		It target;
		
		ShapeRefListIterator(const It& t) : target(t) {}
		
		bool operator==(const Self& other) const { return target == other.target; }
		bool operator!=(const Self& other) const { return target != other.target; }
		bool operator*() const { return *(target -> target); }
		
		Self& operator++() { ++target; return *this; }
		
		const Target& operator*() { return *(target -> target); }
	};
	
	// An implementation of the TreeNode concept
	
	template<typename T>
	struct Node {
		Box<point_for<T::Point>> box;
		virtual ~Node() {}
		
	private:
		Node() {};
		
		friend class LeafNode<T>;
		friend class InteriorNode<T>;
	};
	
	template<typename T>
	struct LeafNode : public Node<T> {
		std::vector<T> data;
	};
	
	template<typename T>
	struct InteriorNode : public Node<T> {
		std::vector<ShapeRef<Node<T>>> children;
	}
	
	template<typename Visitor, typename T>
	Visitor::ReturnType visit(const Node<T>& node, Visitor& v) {
		LeafNode<T>* leaf_ptr = std::dynamic_cast<const LeafNode<T>*>(&node);
		if(leaf_ptr != std::nullptr) {
			v.visit_leaf(leaf_ptr -> data.begin(), leaf_ptr -> data.end());
			return;
		}
		
		InteriorNode<T>& interior = std::dynamic_cast<const InteriorNode<T>&>(node); // This could in principle be static
		using It = decltype(interior.children.begin());
		ShapeRefListIterator<It> begin(interior.children.begin());
		ShapeRefListIterator<It> end(interior.children.end());
		v.visit_interior(begin, end);
	}
	
	template<typename T>
	const Box<point_for<T::Point>>& bounding_box(const Node<T>& node) { return node.box; }
	
	// A single level of the packing algorithm, always builds a vector of leaf nodes.
	
	template<typename T>
	using PackResult<T> = std::vector<ShapeRef<LeafNode<T>>>;
	
	template<typename It1, typename It2>
	PackResult<It1::value_type> pack_static(It1 begin, It2 end, size_t leaf_size) {
		static_assert(std::is_same<It1::value_type, It2::value_type>)
		
		using T = It1::value_type;
		static_assert(implements<T, Shape<T::Point>>::value);
		
		using P = T::Point;
		static constexpr size_t dim = P::dimension;
		
		// Allocate storage for leaf data and points
		using PairType = std::pair<const T&, point_for<T::Point>>;
		std::vector<PairType> storage;
		storage.reserve(std::distance(begin, end));
		
		// Reference inputs and cache bounding box center in storage
		auto transformer = [](const T& in) {
			return PairType(in, center(bounding_box(in)));
		};
		std::transform(begin, end, std::back_inserter(storage), transformer);
		
		double dfactor = pow(((double) storage.size()) / leaf_size, 1.0d / dim);
		size_t factor = (size_t) dfactor;
		
		if(factor == 0) factor = 1;
		
		// Pre-compute sub-division strategy
		std::vector<std::vector<size_t>> indices(dim);
		
		// Strategy for first dimension is trivial
		indices[0].resize(2);
		indices[0][0] = 0;
		indices[0][1] = storage.size();
		
		// Compute subdivision strategy recursively
		for(size_t i_dim = 1; i_dim < dim; ++i_dim) {
			const std::vector<size_t>& in = indices[i_dim - 1];
			std::vector<size_t>& out = indices[i_dim];
			
			out.resize(1 + factor * (in.size() - 1));
			out[out.size() - 1] = storage.size();
			
			for(size_t i = 0; i < in.size() - 1; ++i) {
				// Let's subdivide the range specified between in[i] and in[i+1] into factor subranges
				size_t in_start = in[i];
				size_t in_end   = in[i+1];
				
				size_t in_count = in_end - in_start;
				
				// Every subrange gets at least in_all elements, but 'remain' sub-ranged need to hold one more
				size_t in_all = in_count / factor;
				size_t remain = in_count - factor * in_all;
				
				// Distribute the elements to subranges
				size_t base = in_start;
				for(size_t j = 0; j < factor; ++j) {
					out[factor * i + j] = base;
					base += j < remain ? in_all + 1 : in_all;
				}
			}
		}
		
		// Execute sorting strategy
		for(size_t i_dim = 0; i_dim < dim; ++i_dim) {
			const std::vector<size_t>& idx = indices[i_dim];
			
			auto comparator = [i_dim](const PairType& p1, const PairType& p2) {
				return p1.second.get(i_dim) < p2.second.get(i_dim);
			};
			
			for(size_t i_el = 0; i_el < idx.size() - 1; ++i_el) {
				auto it1 = storage.begin() + idx[i_el];
				auto it2 = storage.begin() + idx[i_el + 1];
				
				std::sort(it1, it2, comparator);
			}
		}
		
		const std::vector<size_t>& last_stage = indices[dim - 1];
		const size_t n_nodes = last_stage.size() - 1;
		
		PackResult<T> result(n_nodes);
		for(size_t i = 0; i < n_nodes; ++i) {
			size_t start = last_stage[i];
			size_t stop  = last_stage[i+1];
			
			auto node = std::make_shared<LeafNode<T>>();
			result[i].target = node;
			
			node -> data.insert(result.begin(), storage.begin() + start, storage.begin() + stop);
			
			// Bounding box computation
			node.data.box = Box<P>::empty();
			for(size_t i = 0; i < result[i]; ++i) {
				node.box = combine_boxes(node.box)
			}
		}
		
		return result;		
	}
	
	// Multi-level packing algorithm, that also erases the node types
	
	template<typename It1, typename It2>
	ShapeRef<Node<It1::value_type>> pack(It1 it1, It2 it2) {
		using T = It1::value_type;
		using NodeList = PackTypeErasure<T>::NodeList;
		
		NodeList nodes = PackTypeErasure<T>::convert(pack_static(it1, it2));
		while(nodes.size() != 1) {
			nodes = PackTypeErasure<T>::convert(pack_static(nodes.begin(), nodes.end()));
		}
		
		return nodes[0];
	}
	
	/** pack_static only can construct leaf nodes. This class contains the method that pack the
	 *  nodes holding the data into the correct target. If it is leaf nodes holding data, the target is
	 *  unchanged. However, if the leaf nodes hold references to other nodes, they are converted into
	 *  interior nodes. */
	template<typename T>
	struct PackTypeErasure {
		using NodeList = std::vector<ShapeRef<Node<T>>>;
		
		/** Simple case: We packed a list of input data */
		static NodeList convert(PackResult<T>&& in) {
			NodeList out(in.size());
			
			for(size_t i = 0; i < in.size(); ++i)
				out[i].target = std::static_pointer_cast<Node<T>>(in[i].target);
			
			return out;
		}
		
		/** Complex case: We packed a list of nodes (or more specificially: ShapeRefs to node T). Here, we need to
		 *  convert the returned leaf nodes into interior nodes.*/
		static NodeList convert(PackResult<ShapeRef<Node<T>>>&& in) {
			NodeList out(in.size());
			
			for(size_t i = 0; i < in.size();++i) {
				std::shared_ptr<InteriorNode<T>> wrapper = std::make_shared<InteriorNode<T>>();
				wrapper.children = in[i].data;
				wrapper.box = in[i].box;
				
				out[i].target = std::static_pointer_cast<Node<T>> wrapper;
			}
			
			return out;
		}
	}
}