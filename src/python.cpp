#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <tinyrgeo/buffer.h>

namespace py = pybind11;
namespace tr = tinyrgeo;

// === Python-compatible triangle mesh types ===

template<typename Num>
struct PyArrayBuffer {
	using Type = Num;
	
	py::array_t<Num> data;
	
	Num& operator()(size_t i, size_t j) { return *data.mutable_data(i, j); }
	const Num& operator()(size_t i, size_t j) const { return *data.data(i, j); }
	
	PyArrayBuffer(py::array_t<Num>& data) : data(data) {}
};

struct PyArrayTriangleMeshBase {
	virtual py::array& get_data() = 0;
	virtual py::array& get_idx()  = 0;
	virtual size_t get_dim() = 0;
};

template<size_t dim, typename Num, typename Idx>
struct PyArrayTriangleMesh :
	public PyArrayTriangleMeshBase,
	public tr::TriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>>
{
	using MeshType = tr::TriangleMesh<dim, PyArrayBuffer<Num>, PyArrayBuffer<Idx>>;
	
	using typename MeshType::Point;
	using typename MeshType::Accessor;
	
	PyArrayTriangleMesh(py::array_t<Num>& data, py::array_t<Idx>& indices) :
		MeshType(PyArrayBuffer<Num>(data), PyArrayBuffer<Idx>(indices))
	{}
	
	py::array& get_data() override { return this->point_buffer.data; }
	py::array& get_idx()  override { return this->index_buffer.data; }
	size_t get_dim() override { return dim; }
};

// === Mesh construction ===

std::shared_ptr<PyArrayTriangleMeshBase> make_mesh(py::array data, py::array_t<std::uint32_t> indices) {
	if(data.ndim() != 2)
		throw std::exception("'data' array must be 2D");
	if(indices.ndim() != 2)
		throw std::exception("'indices' array must be 2D");
	if(indices.shape(1) != 3)
		throw std::exception("'indices' must be of shape [:,3]");
	
	size_t dim = data.shape(1);
	pybind11::dtype dtype = data.dtype();
	
	if(dtype == pybind11::dtype("float32")) {
		switch(dim) {
			case 1: return std::make_shared<PyArrayTriangleMesh<1, float, std::uint32_t>>(py::array_t<float>(data), indices);
			case 2: return std::make_shared<PyArrayTriangleMesh<2, float, std::uint32_t>>(py::array_t<float>(data), indices);
			case 3: return std::make_shared<PyArrayTriangleMesh<3, float, std::uint32_t>>(py::array_t<float>(data), indices);
		}
		
		throw std::exception("Unsupported dimension");
	} else if(dtype == pybind11::dtype("float64")) {
		switch(dim) {
			case 1: return std::make_shared<PyArrayTriangleMesh<1, double, std::uint32_t>>(py::array_t<double>(data), indices);
			case 2: return std::make_shared<PyArrayTriangleMesh<2, double, std::uint32_t>>(py::array_t<double>(data), indices);
			case 3: return std::make_shared<PyArrayTriangleMesh<3, double, std::uint32_t>>(py::array_t<double>(data), indices);
		}
		
		throw std::exception("Unsupported dimension");
	}
	
	throw std::exception("Unkown dtype for 'data'. Must be either 32 or 64 bit float");
}

// === Python module ===

template<int dim, typename Num, typename Idx, typename M>
void register_trimesh(std::string name, M& m) {
	using Root = PyArrayTriangleMesh<dim, Num, Idx>;
	
	py::class_<typename Root::Point>(m, (name + "_point").c_str())
		.def("__getitem__", &Root::Point::operator[]);
	
	py::class_<typename Root::Accessor>(m, (name + "_triangle").c_str())
		.def("__getitem__", &Root::Accessor::operator[]);
		
	py::class_<Root, PyArrayTriangleMeshBase>(m, name.c_str())
		.def(py::init<py::array_t<Num>&, py::array_t<Idx>&>())
		.def("__getitem__", &Root::operator[]);
};

PYBIND11_MODULE(tinyrgeo_python, m) {
	py::class_<PyArrayTriangleMeshBase>(m, "ArrayMesh")
		.def_property_readonly("data", &PyArrayTriangleMeshBase::get_data)
		.def_property_readonly("indices", &PyArrayTriangleMeshBase::get_idx)
		.def_property_readonly("dim", &PyArrayTriangleMeshBase::get_dim);
	
	register_trimesh<1, float, std::uint32_t>("ArrayMesh32_1", m);
	register_trimesh<2, float, std::uint32_t>("ArrayMesh32_2", m);
	register_trimesh<3, float, std::uint32_t>("ArrayMesh32_3", m);
	
	register_trimesh<1, double, std::uint32_t>("ArrayMesh64_1", m);
	register_trimesh<2, double, std::uint32_t>("ArrayMesh64_2", m);
	register_trimesh<3, double, std::uint32_t>("ArrayMesh64_3", m);
	
	m.def("mesh", make_mesh);
}