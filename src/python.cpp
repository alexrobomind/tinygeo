#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <tinygeo/buffer.h>
#include <tinygeo/raytrace.h>
#include <tinygeo/capnp.h>

// POSIX-style file-handling
#include <kj/miniposix.h>

#include <capnp/serialize.h>

namespace py = pybind11;
namespace tr = tinygeo;

// === Python-compatible triangle mesh types ===

template<typename Num>
struct PyArrayBuffer {
	using Type = Num;
	using Ref = Num&;
	
	py::array_t<Num> data;	
	
	Num& operator()(size_t i, size_t j) { return *data.mutable_data(i, j); }
	const Num& operator()(size_t i, size_t j) const { return *data.data(i, j); }
	
	size_t shape(size_t i) const { return data.shape(i); }
	
	PyArrayBuffer(const py::array_t<Num>& data) : data(data) {}
	PyArrayBuffer(const size_t m, const size_t n) : data({m, n}) {}
};

struct PyArrayTriangleMeshBase {
	virtual py::array& get_data() = 0;
	virtual py::array& get_idx()  = 0;
	virtual py::array& get_tags() = 0;
	virtual size_t get_dim() = 0;
	virtual void py_pack(size_t size) = 0;
	virtual void save(const std::string& fname) = 0;
	
	virtual std::vector<size_t> get_grid_size() = 0;
	virtual void set_grid_size(const std::vector<size_t>&) = 0;
	
	virtual ~PyArrayTriangleMeshBase() {}
};

template<size_t dim, typename Num, typename Idx, typename Tag>
struct PyArrayTriangleMesh :
	public PyArrayTriangleMeshBase,
	public tr::IndexedTriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>, PyArrayBuffer<Tag>, tr::SimpleNodeData<tr::Point<dim, Num>>, tr::SimpleGridData>
{
	using MeshType = tr::IndexedTriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>, PyArrayBuffer<Tag>, tr::SimpleNodeData<tr::Point<dim, Num>>, tr::SimpleGridData>;
	
	using typename MeshType::Point;
	using typename MeshType::Accessor;
	
	using InlinePoint = tr::point_for<Point>;
	
	PyArrayTriangleMesh(const py::array_t<Num>& data, const py::array_t<Idx>& indices, const py::array_t<Tag>& tags) :
		MeshType(PyArrayBuffer<Num>(data), PyArrayBuffer<Idx>(indices), PyArrayBuffer<Tag>(tags), tr::SimpleNodeData<InlinePoint>(), tr::SimpleGridData())
	{
		// Create a single R-Tree node with no children, holding all triangles
		this -> root_data.set_start(0);
		this -> root_data.set_end(indices.shape(0));
		this -> root_data.init_children(0);
		
		// Compute root bounding box
		using Box = tr::Box<InlinePoint>;
		Box bb = Box::empty();
		
		for(auto it = this -> begin(); it != this -> end(); ++it) {
			bb = tr::combine_boxes(bb, it->bounding_box());
		}
		
		this -> root_data.bounding_box() = bb; // bounding_box returns a reference for SimpleNodeData
	}
	
	py::array& get_data() override { return this->point_buffer.data; }
	py::array& get_idx()  override { return this->index_buffer.data; }
	py::array& get_tags() override { return this->tag_buffer.data; }
	
	size_t get_dim() override { return dim; }
	void py_pack(size_t size) override { this -> pack(size); }
	
	std::vector<size_t> get_grid_size() override {
		return std::vector<size_t>(this -> grid.size.begin(), this -> grid.size.end());
	}
	
	void set_grid_size(const std::vector<size_t>& new_size) override {
		if(new_size.size() != dim) {
			throw std::invalid_argument("New grid size must have dimension " + std::to_string(dim) + " but has dimension " + std::to_string(new_size.size()));
		}
		
		for(size_t d = 0; d < dim; ++d)
			this -> grid.size[d] = new_size[d];
	}
	
	//TODO: Move this into a templated method
	void save(const std::string& fname) override {
		// Build the serial representation
		capnp::MallocMessageBuilder builder;
		
		auto root = builder.initRoot<tr::capnp::GeoFile>();
		tr::capnp::save_mesh(*this, root);
		
		// Write it out
		
		#if _WIN32 && !__MINGW32__
		// Thanks Microsoft for inventing your "own" API -.-
		const int fd = _open(fname.c_str(), _O_CREAT | _O_TRUNC |_O_BINARY | _O_RDWR, _S_IWRITE);
		#else
		const int fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_BINARY | O_RDWR);
		#endif
		
		capnp::writeMessageToFd(fd, builder);
		
		// Provided on Win32 by kj/miniposix.h
		close(fd);
	}
};

// === Mesh construction ===

std::unique_ptr<PyArrayTriangleMeshBase> make_mesh(py::array data, py::array_t<std::uint32_t> indices, py::array_t<std::uint32_t> tags) {
	if(data.ndim() != 2)
		throw std::runtime_error("'data' array must be 2D");
	if(indices.ndim() != 2)
		throw std::runtime_error("'indices' array must be 2D");
	if(indices.shape(1) != 3)
		throw std::runtime_error("'indices' must be of shape [:,3]");
	if(tags.ndim() != 2)
		throw std::runtime_error("'tags' must be 2D");
	if(tags.shape(1) != 0 && tags.shape(0) != indices.shape(0))
		throw std::runtime_error("'tags' and 'indices' must have same first dimension");
	
	size_t dim = data.shape(1);
	pybind11::dtype dtype = data.dtype();
	
	if(tags.shape(1) == 0) {
		tags.resize({(size_t) indices.shape(0), (size_t) 0});
	}
	
	using index = std::uint32_t;
	
	if(dtype.is(pybind11::dtype("float32"))) {
		switch(dim) {
			case 1: return std::make_unique<PyArrayTriangleMesh<1, float, index, index>>(py::array_t<float>(data), indices, tags);
			case 2: return std::make_unique<PyArrayTriangleMesh<2, float, index, index>>(py::array_t<float>(data), indices, tags);
			case 3: return std::make_unique<PyArrayTriangleMesh<3, float, index, index>>(py::array_t<float>(data), indices, tags);
		}
		
		throw std::runtime_error("Unsupported dimension");
	} else if(dtype.is(pybind11::dtype("float64"))) {
		switch(dim) {
			case 1: return std::make_unique<PyArrayTriangleMesh<1, double, index, index>>(py::array_t<double>(data), indices, tags);
			case 2: return std::make_unique<PyArrayTriangleMesh<2, double, index, index>>(py::array_t<double>(data), indices, tags);
			case 3: return std::make_unique<PyArrayTriangleMesh<3, double, index, index>>(py::array_t<double>(data), indices, tags);
		}
		
		throw std::runtime_error("Unsupported dimension");
	}
	
	throw std::runtime_error("Unkown dtype for 'data'. Must be either 32 or 64 bit float");
}

// === Python module ===

template<typename P, typename M>
auto register_point(std::string name, M& m) {
	return py::class_<P>(m, name.c_str())
		.def("__getitem__", [](const P& p, size_t i) { return p[i]; })
		.def("__setitem__", [](P& p, size_t i, typename P::numeric_type val) { p[i] = val; })
		.def("__len__", [](const P& p) { return P::dimension; })
		.def("__repr__", [name](const P& p) {
			std::string result = "<" + name + ": ";
			
			for(size_t i = 0; i < P::dimension; ++i)
				result += std::to_string(p[i]) + ", ";
			
			result += ">";
			
			return result;
		})
	;
}

template<typename Mesh, typename... Options, typename Num = typename Mesh::Point::numeric_type, typename P = tr::point_for<typename Mesh::Node::Point>>
std::enable_if_t<Mesh::Point::dimension == 3> register_ray_cast(py::class_<Mesh, Options...>& cls) {
	static_assert(std::is_standard_layout<P>::value, "P must be standard layout");
	static_assert(std::is_trivial<P>::value, "P must be trivial");
	
	cls.def("ray_cast", py::vectorize([](Mesh& m, P p1, P p2, Num l_max) {		
		return tr::ray_trace<typename Mesh::Node>(p1, p2, m.root(), l_max).lambda;
	}));
	cls.def("ray_cast_grid", py::vectorize([](Mesh& m, P p1, P p2, Num l_max) {		
		return tr::ray_trace<typename Mesh::Grid>(p1, p2, m.grid, l_max).lambda;
	}));
	
	cls.def("ray_cast_detail", [](Mesh& m, P p1, P p2, Num l_max) {		
		return tr::ray_trace<typename Mesh::Node>(p1, p2, m.root(), l_max);
	});
	cls.def("ray_cast_detail_grid", [](Mesh& m, P p1, P p2, Num l_max) {		
		return tr::ray_trace<typename Mesh::Grid>(p1, p2, m.grid, l_max);
	});
}

template<typename Num, typename Tag, typename M>
void register_ray_cast_result(std::string name, M& m) {
	using R = tr::RaytraceResult<Num, Tag>;
	
	py::class_<R>(m, name.c_str())
		.def_readwrite("lambda", &R::lambda)
		.def_readwrite("tags", &R::tags)
	;
};

template<typename Mesh, typename... Options>
std::enable_if_t<Mesh::Point::dimension != 3> register_ray_cast(py::class_<Mesh, Options...>& cls) {
}

template<size_t dim, typename Num, typename Idx, typename M>
void register_trimesh(std::string name, M& m) {
	using Root = PyArrayTriangleMesh<dim, Num, Idx, Idx>;
	
	register_point<typename Root::Point>(name + "_buf_point", m);
	
	py::class_<typename Root::Accessor>(m, (name + "_buf_triangle").c_str())
		.def("__getitem__", &Root::Accessor::operator[], py::keep_alive<0, 1>())
		.def("__len__", [](const typename Root::Accessor& p) { return 3; })
	;
			
	auto mesh_class = py::class_<Root, PyArrayTriangleMeshBase>(m, name.c_str())
		.def(py::init<py::array_t<Num>&, py::array_t<Idx>&, py::array_t<Idx>&>())
		.def("__getitem__", &Root::operator[], py::keep_alive<0, 1>())
		.def("__len__", &Root::size)
		.def_readonly("root", &Root::root_data)
	;
	register_ray_cast(mesh_class);
};

template<size_t dim, typename M>
void register_cp_trimesh(std::string name, M& m) {
	using CP = tr::CapnpTriangleMesh<dim, double, uint32_t, uint32_t>;
	
	register_point<typename CP::Point>(name + "_buf_point", m);
	
	py::class_<typename CP::Accessor>(m, (name + "_buf_triangle").c_str())
		.def("__getitem__", &CP::Accessor::operator[], py::keep_alive<0, 1>())
		.def("__len__", [](const typename CP::Accessor& p) { return 3; })
	;
			
	auto capnp_mesh_class = py::class_<CP, std::shared_ptr<CP>>(m, name.c_str())
		.def(py::init(&CP::load))
		.def("__getitem__", &CP::operator[], py::keep_alive<0, 1>())
		.def("__len__", &CP::size)
		.def_readonly("root", &CP::root_data)
	;
	register_ray_cast(capnp_mesh_class);
};

template<size_t dim, typename Num>
void register_dimnum(std::string name, py::module_& m) {
	using P = tr::Point<dim, Num>;
	
	auto cls = register_point<P>("Point" + name, m);
	PYBIND11_NUMPY_DTYPE(P, x);
	cls.def_property_readonly_static("dtype", [](py::object type){ return py::dtype::of<P>(); });
	
	using ND = tr::SimpleNodeData<P>;
	py::class_<ND>(m, ("NodeData" + name).c_str())
		.def_property_readonly("range", &ND::range)
		.def_property_readonly("children", [](const ND& in){
			std::vector<ND> children(in.n_children());
			for(size_t i = 0; i < children.size(); ++i)
				children[i] = in.child(i);
			return children;
		})
		.def_property_readonly("box", [](const ND& nd) { return nd.bounding_box(); })
	;
	
	using CPND = tr::CapnpNodeData<P>;
	py::class_<CPND>(m, ("CapnpNodeData" + name).c_str())
		.def_property_readonly("range", &CPND::range)
		.def_property_readonly("children", [](const CPND& in){
			std::vector<CPND> children(in.n_children());
			for(size_t i = 0; i < children.size(); ++i)
				children[i] = in.child(i);
			return children;
		})
		.def_property_readonly("box", [](const CPND& nd) { return nd.bounding_box(); })
	;
	
	using B = tr::Box<P>;
	py::class_<B>(m, ("Box" + name).c_str())
		.def_property_readonly("min", [](const B& b) { return b.min(); })
		.def_property_readonly("max", [](const B& b) { return b.max(); })
	;
	
	register_trimesh<dim, Num, std::uint32_t>("ArrayMesh" + name, m);
}

PYBIND11_MODULE(tinygeo, m) {
	py::class_<PyArrayTriangleMeshBase>(m, "ArrayMesh")
		.def_property_readonly("data", &PyArrayTriangleMeshBase::get_data)
		.def_property_readonly("indices", &PyArrayTriangleMeshBase::get_idx)
		.def_property_readonly("tags", &PyArrayTriangleMeshBase::get_tags)
		.def_property_readonly("dim", &PyArrayTriangleMeshBase::get_dim)
		
		.def_property("grid_size", &PyArrayTriangleMeshBase::get_grid_size, &PyArrayTriangleMeshBase::set_grid_size)
		
		.def("pack", &PyArrayTriangleMeshBase::py_pack)
		.def("save", &PyArrayTriangleMeshBase::save)
	;
	
	register_dimnum<1, float>("32_1", m);
	register_dimnum<2, float>("32_2", m);
	register_dimnum<3, float>("32_3", m);
	
	register_dimnum<1, double>("64_1", m);
	register_dimnum<2, double>("64_2", m);
	register_dimnum<3, double>("64_3", m);
	
	register_cp_trimesh<1>("Capnp_64_1", m);
	register_cp_trimesh<2>("Capnp_64_2", m);
	register_cp_trimesh<3>("Capnp_64_3", m);
	
	register_ray_cast_result<float , uint32_t>("RaycastResult_32", m);
	register_ray_cast_result<double, uint32_t>("RaycastResult_64", m);
	
	py::array_t<uint32_t> tags_default(py::array::ShapeContainer({(size_t) 0, (size_t) 0}));
	m.def("mesh", make_mesh, py::arg("vertices"), py::arg("indices"), py::arg("tags") = tags_default);
}