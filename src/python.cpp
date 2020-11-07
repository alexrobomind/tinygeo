#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <tinygeo/buffer.h>
#include <tinygeo/raytrace.h>

namespace py = pybind11;
namespace tr = tinygeo;

// === Python-compatible triangle mesh types ===

template<typename Num>
struct PyArrayBuffer {
	using Type = Num;
	
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
	virtual size_t get_dim() = 0;
	virtual void py_pack(size_t size) = 0;
	
	virtual ~PyArrayTriangleMeshBase() {}
};

template<size_t dim, typename Num, typename Idx>
struct PyArrayTriangleMesh :
	public PyArrayTriangleMeshBase,
	public tr::IndexedTriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>, tr::SimpleNodeData<tr::Point<dim, Num>>>
{
	using MeshType = tr::IndexedTriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>, tr::SimpleNodeData<tr::Point<dim, Num>>>;
	
	using typename MeshType::Point;
	using typename MeshType::Accessor;
	
	using InlinePoint = tr::point_for<Point>;
	
	PyArrayTriangleMesh(const py::array_t<Num>& data, const py::array_t<Idx>& indices) :
		MeshType(PyArrayBuffer<Num>(data), PyArrayBuffer<Idx>(indices), tr::SimpleNodeData<InlinePoint>())
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
	size_t get_dim() override { return dim; }
	void py_pack(size_t size) override { this -> pack(size); }
};

// === Mesh construction ===

std::unique_ptr<PyArrayTriangleMeshBase> make_mesh(py::array data, py::array_t<std::uint32_t> indices) {
	if(data.ndim() != 2)
		throw std::runtime_error("'data' array must be 2D");
	if(indices.ndim() != 2)
		throw std::runtime_error("'indices' array must be 2D");
	if(indices.shape(1) != 3)
		throw std::runtime_error("'indices' must be of shape [:,3]");
	
	size_t dim = data.shape(1);
	pybind11::dtype dtype = data.dtype();
	
	using index = std::uint32_t;
	
	if(dtype.is(pybind11::dtype("float32"))) {
		switch(dim) {
			case 1: return std::make_unique<PyArrayTriangleMesh<1, float, index>>(py::array_t<float>(data), indices); break;
			case 2: return std::make_unique<PyArrayTriangleMesh<2, float, index>>(py::array_t<float>(data), indices); break;
			case 3: return std::make_unique<PyArrayTriangleMesh<3, float, index>>(py::array_t<float>(data), indices); break;
		}
		
		throw std::runtime_error("Unsupported dimension");
	} else if(dtype.is(pybind11::dtype("float64"))) {
		switch(dim) {
			case 1: return std::make_unique<PyArrayTriangleMesh<1, double, index>>(py::array_t<double>(data), indices);
			case 2: return std::make_unique<PyArrayTriangleMesh<2, double, index>>(py::array_t<double>(data), indices);
			case 3: return std::make_unique<PyArrayTriangleMesh<3, double, index>>(py::array_t<double>(data), indices);
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
	//static_assert(py::detail::is_pod<P>::value, "P must me POD");
	static_assert(std::is_standard_layout<P>::value, "P must be standard layout");
	static_assert(std::is_trivial<P>::value, "P must be trivial");
	cls.def("ray_cast", py::vectorize([](Mesh& m, P p1, P p2, Num l_max) {
		/*using P = tr::point_for<typename Mesh::Node::Point>;
		
		constexpr size_t dim = P::dimension;
		
		P pp1; P pp2;
		for(size_t i = 0; i < dim; ++i) {
			pp1[i] = *p1.data(i);
			pp2[i] = *p2.data(i);
		}*/
		
		return tr::ray_trace<typename Mesh::Node>(p1, p2, m.root(), l_max);
	}));
}

template<typename Mesh, typename... Options>
std::enable_if_t<Mesh::Point::dimension != 3> register_ray_cast(py::class_<Mesh, Options...>& cls) {
}

template<size_t dim, typename Num, typename Idx, typename M>
void register_trimesh(std::string name, M& m) {
	using Root = PyArrayTriangleMesh<dim, Num, Idx>;
	
	register_point<typename Root::Point>(name + "_buf_point", m);
	
	py::class_<typename Root::Accessor>(m, (name + "_buf_triangle").c_str())
		.def("__getitem__", &Root::Accessor::operator[], py::keep_alive<0, 1>())
		.def("__len__", [](const typename Root::Point& p) { return 3; })
	;
			
	auto mesh_class = py::class_<Root, PyArrayTriangleMeshBase>(m, name.c_str())
		.def(py::init<py::array_t<Num>&, py::array_t<Idx>&>())
		.def("__getitem__", &Root::operator[], py::keep_alive<0, 1>())
		.def("__len__", &Root::size)
		.def_readonly("root", &Root::root_data)
	;
	register_ray_cast(mesh_class);
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
		.def_property_readonly("dim", &PyArrayTriangleMeshBase::get_dim)
		.def("pack", &PyArrayTriangleMeshBase::py_pack)
	;
	
	register_dimnum<1, float>("32_1", m);
	register_dimnum<2, float>("32_2", m);
	register_dimnum<3, float>("32_3", m);
	
	register_dimnum<1, double>("64_1", m);
	register_dimnum<2, double>("64_2", m);
	register_dimnum<3, double>("64_3", m);
	
	m.def("mesh", make_mesh);
}