#include <buildsys.h>

//! Used to print out the names of packages in the dependency graph
/** Allows a package to print any information it likes for its label
  */
template<class Name>
class graphnode_property_writer {
	private:
		Name names;
	public:
		//! Construct a property writer
		/** \param _name the mapping from vertices/edges to objects
		  */
		graphnode_property_writer(Name _name) : names(_name) {};
		//! The outputting function
		/** Gets the given vertex or edge label
		  * \param out The stream to write the data to
		  * \param v The vertex or edge to get the data from
		  */
		template <class VertexOrEdge>
		void operator()(std::ostream& out, VertexOrEdge v)
		{
			if(names[v] != NULL)
				names[v]->printLabel(out);
		}
};

Internal_Graph::Internal_Graph()
{
	this->c = NULL;
	// Setup for graphing
	this->Nodes = new NodeVertexMap();
	this->NodeMap = new VertexNodeMap(); 

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

}

void Internal_Graph::output()
{
	std::ofstream dotFile("dependencies.dot");
	write_graphviz(dotFile, g, graphnode_property_writer<VertexNodeMap>(*NodeMap));
        dotFile.flush();
}

void Internal_Graph::topological()
{
	if(c != NULL)
	{
		delete c;
	}
	c = new container();

	topological_sort(this->g, std::back_inserter(*c));

//	std::cout << "A topological ordering: ";
//	for ( container::iterator ii=c->begin(); ii!=c->end(); ++ii)
//		std::cout << ((*NodeMap)[*ii])->getName() << " ";
//	std::cout << std::endl;
}

void Internal_Graph::deleteNode(Package *p)
{
	clear_vertex((*Nodes)[p], this->g);
}

Package *Internal_Graph::topoNext()
{
	Package *n = NULL;
	if(c == NULL) return NULL;
	for ( container::iterator ii=c->begin(); ii!=c->end(); ++ii)
	{
		Package *p = ((*NodeMap)[*ii]);
		if(!(p->isBuilt()) && !(p->isBuilding()) && p->canBuild())
		{
			//fprintf(stderr, "(Possible) Next Pacakge: %s\n", p->getName().c_str());
			n = p;
		}
	}
	return n;
}
