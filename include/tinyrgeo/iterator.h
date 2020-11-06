#pragma once

#include <tinyrgeo/concepts.h>

namespace tinyrgeo {
	
template<typename T, typename F>
class IteratorHelper {
	F converter;
	T acc;
	
	using value_type = T;
	using reference = T&;
	using pointer = T*;
	using difference_type = std::size_t;
	using iterator_category = std::random_access_iterator_tag;
};

}