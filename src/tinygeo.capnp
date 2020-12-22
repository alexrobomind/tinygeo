@0xcf4265a43c62c633; # File GUID

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("tinygeo::capnp");

struct GeoFile {
	header  @0 :Text;
	version @1 :UInt32;
	data    @2 :GeoTree;
}

struct GeoTree {
	data      @0 :List(Float64);
	indices   @1 :List(UInt32);
	tags      @5 :List(UInt32);
	
	dimension @2 :UInt32;
	numTags   @6 :UInt32;
	
	treeRoot  @3 :GeoNode;
	
	grid      @4 :GeoGrid;
}

struct GeoGrid {
	size @0 :List(UInt32);
	data @1 :List(List(UInt32));
}

struct GeoNode {
	boundingBox @0 :GeoBox;
	
	children  @1 :List(GeoNode);
	begin   @2 :UInt32;
	end     @3 :UInt32;
}

struct GeoBox {
	min @0 :List(Float64);
	max @1 :List(Float64);
}