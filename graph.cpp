/******************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "include/buildsys.h"

//! Used to print out the names of packages in the dependency graph
/** Allows a package to print any information it likes for its label
 */
template <class Name> class graphnode_property_writer
{
private:
	Name names;

public:
	//! Construct a property writer
	/** \param _name the mapping from vertices/edges to objects
	 */
	explicit graphnode_property_writer(Name _name) : names(std::move(_name))
	{
	}
	//! The outputting function
	/** Gets the given vertex or edge label
	 * \param out The stream to write the data to
	 * \param v The vertex or edge to get the data from
	 */
	template <class VertexOrEdge> void operator()(std::ostream &out, VertexOrEdge v)
	{
		if(names[v] != nullptr) {
			names[v]->printLabel(out);
		}
	}
};

void Internal_Graph::fill(World *W)
{
	for(auto N = W->nameSpacesStart(); N != W->nameSpacesEnd(); N++) {
		for(auto I = (*N).packagesStart(); I != (*N).packagesEnd(); I++) {
			NodeVertexMap::iterator pos;
			bool inserted;
			boost::tie(pos, inserted) = Nodes.insert(std::make_pair(*I, Vertex()));
			if(inserted) {
				Vertex u = add_vertex(g);
				pos->second = u;
				NodeMap.insert(std::make_pair(u, *I));
			}
		}
	}

	for(auto N = W->nameSpacesStart(); N != W->nameSpacesEnd(); N++) {
		for(auto I = (*N).packagesStart(); I != (*N).packagesEnd(); I++) {
			for(auto J = (*I)->dependsStart(); J != (*I)->dependsEnd(); J++) {
				Edge e;
				bool inserted;
				boost::tie(e, inserted) =
				    add_edge(Nodes[(*I)], Nodes[(*J).getPackage()], g);
			}
		}
	}
}

void Internal_Graph::output()
{
	std::ofstream dotFile("dependencies.dot");
	write_graphviz(dotFile, g, graphnode_property_writer<VertexNodeMap>(NodeMap));
	dotFile.flush();
}

void Internal_Graph::topological()
{
	this->c.clear();

	topological_sort(this->g, std::back_inserter(c));

	//      std::cout << "A topological ordering: ";
	//      for ( container::iterator ii=c->begin(); ii!=c->end(); ++ii)
	//              std::cout << ((*NodeMap)[*ii])->getName() << " ";
	//      std::cout << std::endl;
}

void Internal_Graph::deleteNode(Package *p)
{
	clear_vertex(Nodes[p], this->g);
}

Package *Internal_Graph::topoNext()
{
	Package *n = nullptr;

	for(unsigned long &ii : c) {
		Package *p = NodeMap[ii];
		if(!(p->isBuilt()) && !(p->isBuilding()) && p->canBuild()) {
			// fprintf(stderr, "(Possible) Next Pacakge: %s\n", p->getName().c_str());
			n = p;
		}
	}
	return n;
}
