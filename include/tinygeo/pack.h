#pragma once

#include <vector>
#include <utility>
#include <algorithm>

#include <tinygeo/point.h>
#include <tinygeo/box.h>

#if 0
	template<typename T>
	void packprint(const T& t) {
		py::print(t);
	}
#else
	template<typename T>
	void packprint(const T& t) {}
#endif

namespace tinygeo {
	
	// A simple placeholder node
	template<typename T>
	struct PackNode {
		using Point = typename T::Point;
		
		Box<point_for<typename T::Point>> box;
		
		std::vector<T> data;
		std::vector<PackNode<T>> children;
		
		const Box<point_for<typename T::Point>>& bounding_box() const { return box; }
	};
	
	// A single level of the packing algorithm.
	
	template<typename T>
	using PackResult = std::vector<PackNode<T>>;
	
	template<typename It1, typename It2>
	PackResult<typename It1::value_type> pack_static(It1 begin, It2 end, size_t leaf_size) {
		//static_assert(std::is_same<typename It1::value_type, typename It2::value_type>::value);
		
		using T = typename It1::value_type;		
		using P = typename T::Point;
		static constexpr size_t dim = P::dimension;
		
		// Allocate storage for leaf data and center points
		using PairType = std::pair<T, point_for<typename T::Point>>;
		std::vector<PairType> storage;
		storage.reserve(std::distance(begin, end));
		
		// Copy inputs and cache bounding box center
		{
			auto transformer = [](const T& in) {
				return PairType(in, center(in.bounding_box()));
			};
			std::transform(begin, end, std::back_inserter(storage), transformer);
		}
		
		std::vector<size_t> indirections(storage.size());
		for(size_t i = 0; i < storage.size(); ++i)
			indirections[i] = i;
		
		double dfactor = pow(((double) storage.size()) / leaf_size, 1.0 / dim);
		size_t factor = (size_t) dfactor;
		
		if(factor == 0) factor = 1;
		
		// Pre-compute sub-division strategy
		std::vector<std::vector<size_t>> indices(dim + 1);
		
		// Strategy for first dimension is trivial
		indices[0].resize(2);
		indices[0][0] = 0;
		indices[0][1] = storage.size();
		
		// Compute subdivision strategy recursively
		for(size_t i_dim = 1; i_dim <= dim; ++i_dim) {
			packprint("Computing strategy for dimension " + std::to_string(i_dim));
			
			const std::vector<size_t>& in = indices[i_dim - 1];
			std::vector<size_t>& out = indices[i_dim];
			
			out.resize(1 + factor * (in.size() - 1));
			out[out.size() - 1] = storage.size();
			
			packprint("\tCreated " + std::to_string(out.size() - 1) + " sub-ranges");
			
			for(size_t i = 0; i < in.size() - 1; ++i) {
				// Let's subdivide the range specified between in[i] and in[i+1] into factor subranges
				size_t in_start = in[i];
				size_t in_end   = in[i+1];
				
				packprint("\t\tProcessing range [" + std::to_string(in_start) + ", " + std::to_string(in_end) + "[");
				
				size_t in_count = in_end - in_start;
				
				// Every subrange gets at least in_all elements, but 'remain' sub-ranged need to hold one more
				size_t in_all = in_count / factor;
				size_t remain = in_count - factor * in_all;
				
				// Distribute the elements to subranges
				size_t base = in_start;
				for(size_t j = 0; j < factor; ++j) {
					packprint("\t\t\t" + std::to_string(factor * i + j) + " <- " + std::to_string(base));
					
					out[factor * i + j] = base;
					base += j < remain ? in_all + 1 : in_all;
				}
			}
		}
		
		// Execute sorting strategy
		for(size_t i_dim = 0; i_dim < dim; ++i_dim) {
			packprint("Sorting dimension " + std::to_string(i_dim));
			const std::vector<size_t>& idx = indices[i_dim];
			
			auto comparator = [i_dim, &storage](size_t i1, size_t i2) {
				const PairType& p1 = storage[i1];
				const PairType& p2 = storage[i2];
				return p1.second[i_dim] < p2.second[i_dim];
			};
			
			for(size_t i_el = 0; i_el < idx.size() - 1; ++i_el) {
				auto it1 = indirections.begin() + idx[i_el];
				auto it2 = indirections.begin() + idx[i_el + 1];
				
				std::sort(it1, it2, comparator);
			}
		}
		
		const std::vector<size_t>& last_stage = indices[dim];
		const size_t n_nodes = last_stage.size() - 1;
		
		PackResult<T> result(n_nodes);
		packprint("Copying output");
		for(size_t i = 0; i < n_nodes; ++i) {
			size_t start = last_stage[i];
			size_t stop  = last_stage[i+1];
			
			auto& node = result[i];
			
			// Copy the data into the target node
			{
				node.data.reserve(stop - start);
				auto transformer = [&](size_t i) {
					const PairType& in = storage[i];
					return in.first;
				};
				std::transform(indirections.begin() + start, indirections.begin() + stop, std::back_inserter(node.data), transformer);
			}
			
			// Bounding box computation
			auto box = Box<P>::empty();
			for(size_t j = start; j < stop; ++j) {
				box = combine_boxes(box, storage[indirections[j]].first.bounding_box());
			}
			node.box = box;
		}
		
		return result;		
	}
		
	/** pack_static only can construct leaf nodes. This class contains the method that pack the
	 *  nodes holding the data into the correct target. If it is leaf nodes holding data, the target is
	 *  unchanged. However, if the leaf nodes hold references to other nodes, they are converted into
	 *  interior nodes. */
	template<typename T>
	struct PackNesting {
		using NodeList = std::vector<PackNode<T>>;
		
		/** Simple case: We packed a list of input data */
		static NodeList convert(PackResult<T>&& in) {
			return in;
		}
		
		/** Complex case: We packed a list of nodes. Here, we need to
		 *  convert the returned leaf nodes into interior nodes.*/
		static NodeList convert(PackResult<PackNode<T>>&& in) {
			NodeList out(in.size());
			
			for(size_t i = 0; i < in.size();++i) {
				out[i].children = std::move(in[i].data);
				out[i].box = std::move(in[i].box);
			}
			
			return out;
		}
	};
	
	template<typename It1, typename It2>
	PackNode<typename It1::value_type> pack(It1 it1, It2 it2, size_t size) {
		using T = typename It1::value_type;
		using NodeList = typename PackNesting<T>::NodeList;
		
		NodeList nodes = PackNesting<T>::convert(pack_static(it1, it2, size));
		while(nodes.size() != 1) {
			nodes = PackNesting<T>::convert(pack_static(nodes.begin(), nodes.end(), size));
		}
		
		return nodes[0];
	}
}