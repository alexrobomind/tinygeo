#pragma once

#include <list>

#include <tinygeo/pack.h>
#include <tinygeo/triangle.h>
#include <tinygeo/point.h>

#include <capnp/serialize.h>

// POSIX-style file-handling
#if _WIN32
#include <kj/miniposix.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

// Auto-generated Cap'n'proto header
#include <tinygeo.capnp.h>

#include <iostream>

namespace tinygeo {
	
template<typename T>
struct CapnpBufferReader {
	using Type = T;
	using Backend = typename ::capnp::List<T>::Reader;
	
	struct Ref {
		Backend& target;
		size_t idx;
		
		Ref(Backend& target, size_t idx) :
			target(target), idx(idx)
		{}
		
		operator T() const {
			return target[idx];
		}
		
		void operator=(const Type& other) {
			throw std::logic_error("Reader-based capnp buffer can not be written into");
		}
	};
	
	Backend backend;
	std::array<size_t, 2> myshape;
	
	CapnpBufferReader(const Backend backend, std::array<size_t, 2> shape) :
		backend(backend), myshape(shape)
	{
		if(shape[0] * shape[1] != backend.size())
			throw std::invalid_argument("Shape product must be equal to buffer size");
	}
	
	Ref operator()(size_t i, size_t j) {
		size_t li = shape(1) * i + j;
		return Ref(backend, li);
	}
	
	size_t shape(size_t d) const {
		if(d < myshape.size())
			return myshape[d];
		
		throw std::logic_error("Only 2D buffers supported by Capnp backend");
	}
};
	
template<typename P>
struct CapnpNodeData {
	capnp::GeoNode::Reader backend;
	
	CapnpNodeData(const capnp::GeoNode::Reader& backend) :
		backend(backend)
	{}
	
	CapnpNodeData() {}
	
	std::pair<size_t, size_t> range() const { return std::make_pair(backend.getBegin(), backend.getEnd()); }
	void set_start(size_t val) { throw std::logic_error("Cannot set start on Capnp node data"); }
	void set_end(size_t val) {  throw std::logic_error("Cannot set end on Capnp node data"); }
	
	void init_children(size_t s) {  throw std::logic_error("Cannot set children on Capnp node data"); }
	size_t n_children() const { return backend.getChildren().size(); }
	CapnpNodeData<P> child(size_t i) const { return CapnpNodeData(backend.getChildren()[i]); }
	
	Box<P> bounding_box() const {
		Box<P> result;
		
		for(size_t d = 0; d < P::dimension; ++d) {
			result.min()[d] = backend.getBoundingBox().getMin()[d];
			result.max()[d] = backend.getBoundingBox().getMax()[d];
		}
		
		return result;
	}
};

struct CapnpGridData {
	capnp::GeoGrid::Reader backend;
	CapnpGridData(capnp::GeoGrid::Reader backend) :
		backend(backend)
	{}
	
	const ::capnp::List<uint32_t>::Reader get(size_t i) const { 
		return backend.getData()[i];
	}
	
	size_t size() {
		return backend.getData().size();
	}
	
	void reset(size_t n) {
		throw std::logic_error("Can not reset capnp grid data");
	}
	
	void insert(size_t n, size_t val) {
		throw std::logic_error("Can not insert into capnp grid data");
	}
};

template<size_t dim, typename Num, typename Idx, typename Tag>
struct CapnpTriangleMesh :
	public IndexedTriangleMesh<dim, CapnpBufferReader<Num>, CapnpBufferReader<Idx>, CapnpBufferReader<Tag>, CapnpNodeData<Point<dim, Num>>, CapnpGridData>
{
	using MeshType = IndexedTriangleMesh<
		dim,
		CapnpBufferReader<Num>,
		CapnpBufferReader<Idx>,
		CapnpBufferReader<Tag>,
		CapnpNodeData<::tinygeo::Point<dim, Num>>,
		CapnpGridData
	>;
	
	using typename MeshType::Point;
	using typename MeshType::Accessor;
	
	using InlinePoint = point_for<Point>;
	
	CapnpTriangleMesh(capnp::GeoTree::Reader reader) :
		MeshType(
			CapnpBufferReader<Num>(reader.getData(),    {reader.getData().size() / dim , dim}),
			CapnpBufferReader<Idx>(reader.getIndices(), {reader.getIndices().size() / 3, 3  }),
			CapnpBufferReader<Tag>(reader.getTags(),    {reader.getIndices().size() / 3, reader.getNumTags()}),
			CapnpNodeData<::tinygeo::Point<dim, Num>>(reader.getTreeRoot()),
			CapnpGridData(reader.getGrid())
		)
	{
		if(reader.getDimension() != dim) {
			throw std::logic_error("Dimension mismatch between file and reader");
		}
		
		for(size_t d = 0; d < dim; ++d)
			this -> grid.size[d] = reader.getGrid().getSize()[d];
	}
	
	static std::shared_ptr<CapnpTriangleMesh<dim, Num, Idx, Tag>> load(const std::string& filename) {
		#if _WIN32 && !__MINGW32__
		const int fd = _open(filename.c_str(), _O_BINARY | _O_RDONLY);
		#else
		const int fd = open(filename.c_str(), O_RDONLY);
		#endif
		
		::capnp::ReaderOptions options;
		options.traversalLimitInWords = ((uint64_t) 1) << 60;//8 * 1024 * 1024 * 1024;
				
		::capnp::StreamFdMessageReader* message = new ::capnp::StreamFdMessageReader(fd, options);
		
		auto deleter = [=](MeshType* in) {
			delete in;
			delete message;
			close(fd);
		};
		
		return std::shared_ptr<CapnpTriangleMesh<dim, Num, Idx, Tag>>(
			new CapnpTriangleMesh<dim, Num, Idx, Tag>(message -> getRoot<capnp::GeoFile>().getData()),
			deleter
		);
	}
};

namespace capnp {

template<typename B, typename O>
void save_buffer(const B& buf, O out) {
	for(size_t i = 0; i < buf.shape(0); ++i) {
		for(size_t j = 0; j < buf.shape(1); ++j) {
			size_t li = i * buf.shape(1) + j;
			out.set(li, buf(i, j));
		}
	}
}

template<typename N>
void save_node_data(const N& data, GeoNode::Builder target) {
	// Save range
	auto r = data.range();	
	target.setBegin(r.first);
	target.setEnd(r.second);
	
	// Save bounding box
	auto bb = data.bounding_box();
	constexpr size_t dimension = decltype(bb)::Point::dimension;
	
	auto cbb = target.getBoundingBox();
	cbb.initMin(dimension);
	cbb.initMax(dimension);
	
	for(size_t d = 0; d < dimension; ++d) {
		cbb.getMin().set(d, bb.min()[d]);
		cbb.getMax().set(d, bb.max()[d]);
	}
	
	// Save children
	auto children = target.initChildren(data.n_children());
	for(size_t i = 0; i < children.size(); ++i)
		save_node_data(data.child(i), children[i]);
}

template<typename G>
void save_grid_data(const G& data, GeoGrid::Builder target) {
	target.initData(data.size());
	
	for(size_t i = 0; i < data.size(); ++i) {
		auto subd = data.get(i);
		target.getData().init(i, subd.size());
		
		auto it = subd.begin();
		for(size_t j = 0; j < subd.size(); ++j, ++it)
			target.getData()[i].set(j, *it);
	}
}

template<size_t dim, typename PointBuffer, typename IndexBuffer, typename TagBuffer, typename NodeData, typename GridData>
void save_mesh(IndexedTriangleMesh<dim, PointBuffer, IndexBuffer, TagBuffer, NodeData, GridData>& mesh, capnp::GeoTree::Builder out) {
	out.setDimension(dim);
	out.setNumTags(mesh.tag_buffer.shape(1));
	
	save_buffer(mesh.point_buffer, out.initData(mesh.point_buffer.shape(0) * dim));
	save_buffer(mesh.index_buffer, out.initIndices(mesh.index_buffer.shape(0) * 3));
	save_buffer(mesh.tag_buffer,   out.initTags(mesh.tag_buffer.shape(0) * mesh.tag_buffer.shape(1)));
	
	save_node_data(mesh.root_data, out.getTreeRoot());
	save_grid_data(mesh.grid.data, out.getGrid());
	
	out.getGrid().initSize(dim);
	for(size_t d = 0; d < dim; ++d)
		out.getGrid().getSize().set(d, mesh.grid.size[d]);
}

template<size_t dim, typename PointBuffer, typename IndexBuffer, typename TagBuffer, typename NodeData, typename GridData>
void save_mesh(IndexedTriangleMesh<dim, PointBuffer, IndexBuffer, TagBuffer, NodeData, GridData>& mesh, capnp::GeoFile::Builder out) {
	out.setHeader("This file was saved by the tinygeo library. See https://github.com/alexrobomind/tinygeo for the source code and the CapnProto schema for this file.");
	out.setVersion(0);
	
	save_mesh(mesh, out.getData());
}

}

}
