@0xcf4265a43c62c633; # File GUID

struct GeoFile {
	header @0 :Text;
	data   @1 :GeoTree;
}

struct GeoTree {
	data      @0 :List(Float64);
	indices   @1 :List(UInt32);
	dimension @2 :UInt32;
	
	treeRoot @3 :Node;
}

struct Node {
	boundingBox @0 :Box;
	
	children  @1 :List(Node);
	begin   @2 :UInt32;
	end     @3 :UInt32;
}

struct Box {
	min @0 :List(Float64);
	max @1 :List(Float64);
}