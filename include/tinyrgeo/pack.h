namespace tinyrgeo {
	
	// A simple implementation of the Node concept
	template<typename T>
	struct Node {
		Box<point_for<T::Point>> box;
		
		std::vector<T> data;
		std::vector<Node<T>> children;
		
		const Box<point_for<T::Point>>& bounding_box() const { return box; }
	};
	
	// A single level of the packing algorithm, always builds a vector of leaf nodes.
	
	template<typename T>
	using PackResult<T> = std::vector<Node<T>>;
	
	template<typename It1, typename It2>
	PackResult<It1::value_type> pack_static(It1 begin, It2 end, size_t leaf_size) {
		static_assert(std::is_same<It1::value_type, It2::value_type>)
		
		using T = It1::value_type;		
		using P = T::Point;
		static constexpr size_t dim = P::dimension;
		
		// Allocate storage for leaf data and points
		using PairType = std::pair<T, point_for<T::Point>>;
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
			
			auto& node = result[i];
			
			node.data.insert(node.data.begin(), storage.begin() + start, storage.begin() + stop);
			
			// Bounding box computation
			node.data.box = Box<P>::empty();
			for(size_t i = 0; i < result[i]; ++i) {
				node.box = combine_boxes(node.box, node.data[i].bounding_box())
			}
		}
		
		return result;		
	}
		
	/** pack_static only can construct leaf nodes. This class contains the method that pack the
	 *  nodes holding the data into the correct target. If it is leaf nodes holding data, the target is
	 *  unchanged. However, if the leaf nodes hold references to other nodes, they are converted into
	 *  interior nodes. */
	template<typename T>
	struct PackNesting {
		using NodeList = std::vector<Node<T>>;
		
		/** Simple case: We packed a list of input data */
		static NodeList convert(PackResult<T>&& in) {
			return in;
		}
		
		/** Complex case: We packed a list of nodes. Here, we need to
		 *  convert the returned leaf nodes into interior nodes.*/
		static NodeList convert(PackResult<Node<T>>&& in) {
			NodeList out(in.size());
			
			for(size_t i = 0; i < in.size();++i) {
				out[i].children = std::move(in[i].data);
				out[i].box = std::move(in[i].box);
			}
			
			return out;
		}
	}
	
	template<typename It1, typename It2>
	Node<It1::value_type> pack(It1 it1, It2 it2) {
		using T = It1::value_type;
		using NodeList = typename PackNesting<T>::NodeList;
		
		NodeList nodes = PackNesting<T>::convert(pack_static(it1, it2));
		while(nodes.size() != 1) {
			nodes = PackNesting<T>::convert(pack_static(nodes.begin(), nodes.end()));
		}
		
		return nodes[0];
	}
}