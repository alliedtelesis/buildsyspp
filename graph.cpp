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
	for(const auto &ns : W->getNameSpaces()) {
		for(const auto &package : ns.getPackages()) {
			NodeVertexMap::iterator pos;
			bool inserted;
			boost::tie(pos, inserted) =
			    Nodes.insert(std::make_pair(package.get(), Vertex()));
			if(inserted) {
				Vertex u = add_vertex(g);
				pos->second = u;
				NodeMap.insert(std::make_pair(u, package.get()));
			}
		}
	}

	for(const auto &ns : W->getNameSpaces()) {
		for(const auto &package : ns.getPackages()) {
			for(auto &depend : package->getDepends()) {
				Edge e;
				bool inserted;
				boost::tie(e, inserted) =
				    add_edge(Nodes[(package.get())], Nodes[depend.getPackage()], g);
			}
		}
	}
}

void Internal_Graph::output() const
{
	std::ofstream dotFile("dependencies.dot");
	write_graphviz(dotFile, g, graphnode_property_writer<VertexNodeMap>(NodeMap));
	dotFile.flush();
}

void Internal_Graph::topological()
{
	this->c.clear();

	topological_sort(this->g, std::back_inserter(c));
}

void Internal_Graph::deleteNode(Package *p)
{
	clear_vertex(Nodes[p], this->g);
}

Package *Internal_Graph::topoNext()
{
	Package *n = nullptr;

	for(auto &ii : c) {
		Package *p = NodeMap[ii];
		if(!(p->isBuilt()) && !(p->isBuilding()) && p->canBuild()) {
			n = p;
		}
	}
	return n;
}
