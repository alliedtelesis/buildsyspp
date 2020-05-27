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

using namespace boost;

void Internal_Graph::fill()
{
	NameSpace::for_each([this](const NameSpace &ns) {
		ns.for_each_package([this](Package &package) {
			NodeVertexMap::iterator pos;
			bool inserted;
			boost::tie(pos, inserted) =
			    this->Nodes.insert(std::make_pair(&package, Vertex()));
			if(inserted) {
				Vertex u = add_vertex(this->g);
				pos->second = u;
				this->NodeMap.insert(std::make_pair(u, &package));
			}
		});
	});

	NameSpace::for_each([this](const NameSpace &ns) {
		ns.for_each_package([this](Package &package) {
			for(auto &depend : package.getDepends()) {
				Edge e;
				bool inserted;
				boost::tie(e, inserted) = add_edge(
				    this->Nodes[(&package)], this->Nodes[depend.getPackage()], this->g);
			}
		});
	});
}

std::unordered_set<Package *> Internal_Graph::get_cycled_packages() const
{
	struct cycle_detector : public dfs_visitor<> {
		cycle_detector(const VertexNodeMap &_map,
		               std::unordered_set<Package *> *_cycled_packages)
		    : map(_map), packages(_cycled_packages)

		{
		}

		/**
		 * Called if the DFS traverses a back edge (i.e. a cycle is found)
		 */
		void back_edge(Edge e, const Graph &g)
		{
			Vertex vertex1 = source(e, g);
			Vertex vertex2 = target(e, g);

			packages->insert(this->map.at(vertex1));
			packages->insert(this->map.at(vertex2));
		}

		const VertexNodeMap &map;
		std::unordered_set<Package *> *packages;
	};

	std::unordered_set<Package *> cycled_packages;

	cycle_detector vis(this->NodeMap, &cycled_packages);
	depth_first_search(this->g, visitor(vis));

	return cycled_packages;
}

void Internal_Graph::output() const
{
	std::ofstream dotFile("dependencies.dot");
	write_graphviz(dotFile, this->g, VertexNodeMap_property_writer(this->NodeMap));
	dotFile.flush();
}

void Internal_Graph::topological()
{
	this->c.clear();

	topological_sort(this->g, std::back_inserter(this->c));
}

void Internal_Graph::deleteNode(Package *p)
{
	clear_vertex(this->Nodes[p], this->g);
}

Package *Internal_Graph::topoNext()
{
	Package *n = nullptr;

	for(auto &ii : c) {
		Package *p = this->NodeMap[ii];
		if(!(p->isBuilt()) && !(p->isBuilding()) && p->canBuild()) {
			n = p;
		}
	}
	return n;
}
