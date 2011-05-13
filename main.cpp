#include <buildsys.h>

World *buildsys::WORLD;

using namespace boost;

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
typedef boost::graph_traits < Graph >::vertex_descriptor Vertex;
typedef boost::graph_traits < Graph >::edge_descriptor Edge;
typedef std::map<Package *, Vertex > NodeVertexMap;
typedef std::map<Vertex, Package * > VertexNodeMap;

template<class Name>
class graphnode_property_writer {
	private:
		Name names;
	public:
		graphnode_property_writer(Name _name) : names(_name) {};
		template <class VertexOrEdge>
		void operator()(std::ostream& out, VertexOrEdge v)
		{
			if(names[v] != NULL)
				out << "[label=\"" << names[v]->getName() << "\"]";
		}
};

void buildsys::graph()
{
	// Setup for graphing
        NodeVertexMap *Nodes = new NodeVertexMap();
	VertexNodeMap *NodeMap = new VertexNodeMap(); 
        Graph g;

	for(std::list<Package *>::iterator I = WORLD->packagesStart();
		I != WORLD->packagesEnd(); I++)
	{
		NodeVertexMap::iterator pos;
		bool inserted;
		boost::tie(pos, inserted) = Nodes->insert(std::make_pair(*I, Vertex()));
		if(inserted)
		{
			Vertex u = add_vertex(g);
			pos->second = u;
			NodeMap->insert(std::make_pair(u, *I));
		}
	}

	for(std::list<Package *>::iterator I = WORLD->packagesStart();
		I != WORLD->packagesEnd(); I++)
	{
		for(std::list<Package *>::iterator J = (*I)->dependsStart();
			J != (*I)->dependsEnd(); J++)
		{
			Edge e;
			bool inserted;
			boost::tie(e, inserted) = add_edge((*Nodes)[(*I)],(*Nodes)[(*J)],g);
		}
	}

	std::ofstream dotFile("dependencies.dot");
	write_graphviz(dotFile, g, graphnode_property_writer<VertexNodeMap>(*NodeMap));
        dotFile.flush();


}

int main(int argc, char *argv[])
{
	struct timespec start, end;
	
	clock_gettime(CLOCK_REALTIME, &start);

	std::cout << "Buildsys (C++ version)" << std::endl;
	std::cout << "Built: " << __TIME__ << " " << __DATE__ << std::endl;

	
	if(argc <= 1)
	{
		error(std::string("At least 1 parameter is required"));
	}
	
	try {
		WORLD = new World();
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}
	
	try {
		interfaceSetup(WORLD->getLua());
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}
	
	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	int a = 1;
	bool foundDashDash = false;
	while(a < argc && !foundDashDash)
	{
		if(!strcmp(argv[a],"--"))
		{
			foundDashDash = true;
		} else
		{
			WORLD->forceBuild(argv[a]);
		}
		a++;
	}
	// then we find a --
	if(foundDashDash)
	{
		try {
		
			// then we can preload the feature set
			while(a < argc)
			{
				WORLD->setFeature(argv[a]);
				a++;
			}
		}
		catch(Exception &E)
		{
			error(E.error_msg());
		}
	}
	try {
		WORLD->basePackage(argv[1]);
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}
	
	// Graph the dependency tree
	buildsys::graph();

	clock_gettime(CLOCK_REALTIME, &end);
	
	std::cout << "Total time: " << (end.tv_sec - start.tv_sec)  << "s and " << (end.tv_nsec - start.tv_nsec) / 1000 << " ms" << std::endl;
	
	return 0;
}
