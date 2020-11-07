#pragma once

#include <tinygeo/concepts.h>
#include <tinygeo/buffer.h>
#include <Eigen/Dense>

namespace tinygeo {
	
template<typename T>
struct same_type { using type = T; };

template<typename T>
using same_type_t = typename same_type<T>::type;

template<typename N>
std::enable_if_t<N::tag == tags::node, typename N::Point::numeric_type> ray_trace(
	const point_for<typename N::Point>& start,
	const point_for<typename N::Point>& end,
	const N& node,
	typename N::Point::numeric_type l_max
) {
	using Num = typename N::Point::numeric_type;
	using PairType = std::pair<Num, size_t>;
	
	//pybind11::print("Casting ray through node");
	//pybind11::print("  with " + std::to_string(node.n_data()) + " triangles");
	//pybind11::print("  with " + std::to_string(node.n_children()) + " children");
	
	// Intersect with stored triangles
	Num result = std::numeric_limits<Num>::infinity();
	{
		std::vector<PairType> data;
		data.reserve(node.n_data());
		for(size_t i = 0; i < node.n_data(); ++i)
			data.push_back(std::make_pair(ray_trace(start, end, node.data(i).bounding_box(), l_max), i));
		
		std::sort(data.begin(), data.end());
		
		for(size_t i = 0; i < data.size(); ++i) {
			if(data[i].first < std::min(result, l_max)) {
				Num it_result = ray_trace(start, end, node.data(data[i].second), l_max);
				result = std::min(result, it_result);
			} else {
				continue;
			}
		}
	}
	
	//pybind11::print("  after children, result is " + std::to_string(result));
	
	// Compute lower bound on distance based on bounding box intersections
	{
		std::vector<PairType> data;
		data.reserve(node.n_children());
		for(size_t i = 0; i < node.n_children(); ++i)
			data.push_back(std::make_pair(ray_trace(start, end, node.child(i).bounding_box(), l_max), i));
		
		std::sort(data.begin(), data.end());
		
		for(size_t i = 0; i < node.n_children(); ++i) {
			//pybind11::print("  Intersection with bounding box " + std::to_string(data[i].second) + ": " + std::to_string(data[i].first));
			
			if(data[i].first < std::min(result, l_max)) {
				//pybind11::print("  Promising. Proceeding with child");
				Num it_result = ray_trace(start, end, node.child(data[i].second), l_max);
				//pybind11::print("  Child returned " + std::to_string(it_result));
				result = std::min(result, it_result);
				//pybind11::print("  New threshold: " + std::to_string(result));
			} else {
				//pybind11::print("  Limit exceeded. Returning " + std::to_string(result));
				//pybind11::print("  Limit exceeded. Trying anyway");
				//Num it_result = ray_trace(start, end, node.child(data[i].second), l_max);
				//pybind11::print("  Child returned " + std::to_string(it_result));
				continue;
			}
		}
	}
	
	//pybind11::print("  All boxed processed. Returning " + std::to_string(result));
	
	return result;
}

template<typename B>
typename std::enable_if_t<B::tag == tags::box, typename B::Point::numeric_type> ray_trace(
	const point_for<typename B::Point>& start,
	const point_for<typename B::Point>& end,
	const B& box,
	typename B::Point::numeric_type l_max
) {
	using PB = typename B::Point;
	using Num = typename PB::numeric_type;
	constexpr size_t dim = PB::dimension;
	
	const PB p1 = box.min();
	const PB p2 = box.max();
	
	const Num inf = std::numeric_limits<Num>::infinity();
	const Num tol = 5 * std::numeric_limits<Num>::epsilon();
	
	// We need to check for the empty box
	if(is_empty(box))
		return inf;
	
	Num lower_bound = 0;
	Num upper_bound = inf;	
	
	for(size_t i = 0; i < dim; ++i) {
		Num i_low;
		Num i_high;
		
		// In case we don't point in the i'th direction, we have to see whether we are inside the box
		// If we are, any value is OK
		// If we are not, none is
		if(std::abs(end[i] - start[i]) <= tol) {
			if((p1[i] - start[i]) * (p2[i] - start[i]) <= 0) {
				i_low = -inf;
				i_high = inf;
			} else {
				i_low = inf;
				i_high = -inf;
			}
		} else {
			const Num l1 = (p1[i] - start[i]) / (end[i] - start[i]);
			const Num l2 = (p2[i] - start[i]) / (end[i] - start[i]);
			
			if(l1 < l2) {
				i_low = l1; i_high = l2;
			} else {
				i_low = l2; i_high = l1;
			}
		}
		
		// Reduction
		lower_bound = std::max(lower_bound, i_low);
		upper_bound = std::min(upper_bound, i_high);
	}
	
	if(lower_bound > upper_bound)
		return inf;
	
	if(lower_bound > l_max)
		return inf;
	
	return lower_bound;
}

template<typename T>
typename std::enable_if_t<
	T::tag == tags::triangle && T::Point::dimension == 3,
	typename T::Point::numeric_type
> ray_trace(const point_for<typename T::Point>& start, const point_for<typename T::Point>& end, const T& tri, typename T::Point::numeric_type l_max) {
	using P = typename T::Point;
	using Num = typename P::numeric_type;
	using Mat = Eigen::Matrix<Num, 3, 3>;
	using Vec = Eigen::Matrix<Num, 3, 1>;
		
	Mat m;
	for(size_t i = 0; i < 3; ++i) {
		m(i, 0) = end[i] - start[i];
		m(i, 1) = tri.template get<1>()[i] - tri.template get<0>()[i];
		m(i, 2) = tri.template get<2>()[i] - tri.template get<0>()[i];
	}
	
	Vec v;
	for(size_t i = 0; i < 3; ++i) {
		v(i, 0) = start[i] - tri.template get<0>()[i];
	}
	
	Vec vi = m.partialPivLu().solve(v);
	
	const Num l = -vi(0, 0);
	const Num inf = std::numeric_limits<Num>::infinity();
	
	// Check if we didn't hit the triangle plane
	if(l > l_max || l < 0)
		return inf;
	
	// Check if we missed the triangle
	if(vi(1, 0) < 0 || vi(2, 0) < 0 || vi(1, 0) + vi(2, 0) > 1)
		return inf;
	
	return l;
}

template<typename T>
typename std::enable_if_t<
	T::tag == tags::triangle && T::Point::dimension != 3,
	typename T::Point::numeric_type
> ray_trace(const point_for<typename T::Point>& start, const point_for<typename T::Point>& end, const T& tri, typename T::Point::numeric_type l_max) {
	throw std::runtime_error("Not implemented");
}

/*template<typename T>
typename std::enable_if_t<
	T::tag == tags::triangle && T::Point::dimension == 2,
	typename T::Point::numeric_type
> ray_trace(const typename T::Point& start, const typename T::Point& end, const T& tri, typename T::Point::numeric_type l_max) {
	using P = typename T::Point;
	using Num = typename P::numeric_type;
	
	using PMat = Eigen::Matrix<Num, 4, 2>;
	PMat m;
	
	m(0, 0) = end[0] - start[0];
	m(0, 1) = end[1] - start[1];
	m(1, 0) = tri.get<1>[1] - tri.get<0>[1];
	m(1, 1) = tri.get<0>[0] - tri.get<1>[0];
	m(2, 0) = tri.get<2>[1] - tri.get<0>[1];
	m(2, 1) = tri.get<0>[0] - tri.get<2>[0];
	m(3, 0) = tri.get<2>[1] - tri.get<1>[1];
	m(3, 1) = tri.get<1>[0] - tri.get<2>[0];
	
	Eigen::Matrix<Num, 2, 2> eline;
	eline(0, 0) = start[0];
	eline(1, 0) = start[1];
	eline(0, 1) = end[0];
	eline(1, 1) = end[1];
	
	Eigen::Matrix<Num, 2, 3> etri;
	eline(0, 0) = tri.get<0>[0];
	eline(1, 0) = tri.get<0>[1];
	eline(0, 1) = tri.get<1>[0];
	eline(1, 1) = tri.get<1>[1];
	eline(0, 2) = tri.get<2>[0];
	eline(1, 2) = tri.get<2>[1];
	
	auto p_line = m * eline;
	auto p_tri  = m * etri;
	
	if((p_line.rowwise().maxCoeff() > p_tri.rowwise().minCoeff() && p
}*/
}