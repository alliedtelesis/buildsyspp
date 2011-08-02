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
				names[v]->printLabel(out);
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

#ifdef UNDERSCORE
static us_job_queue *jq;
static us_event_set *es;
static us_delay_set *ds;
us_net_connection *buildsys_conn;

static bool buildsys_server_message(us_net_message *m)
{
	fprintf(stderr, "Got message from buildsys server (ID = %x)\n", m->ID);
	us_datacoding_set *ds = us_datacoding_set_create_string(m->sl);
	us_datacoding_data *d = us_datacoding_get(ds);
	switch(m->ID)
	{
		default:
			fprintf(stderr, "Unknown message from build controller\n");
	}
	us_datacoding_set_destroy(ds);
	return true;
}

static bool buildsys_update_metric()
{
	if(buildsys_conn != NULL)
	{
		// get the performance numbers
		meminfo();
		double curr_load;
		loadavg(&curr_load, NULL, NULL);
		// convert them to the metric
		unsigned long long m = kb_main_free;
		unsigned long long l = (unsigned long long)((double)(curr_load * 100.0L));
		us_datacoding_set *ds = us_datacoding_set_create();
		us_datacoding_add_uint(ds, 'M', m);
		us_datacoding_add_uint(ds, 'L', l+1);
		us_string_length sl = us_datacoding_string(ds);
		us_datacoding_set_destroy(ds);
		// send to the buildsys server
		us_net_send(buildsys_conn, sl, 0x10101010);
		free(sl.string);
	}
	return true;
}

static bool buildsys_server_dropped(us_net_connection *nc)
{
	us_core_restart();
	fprintf(stderr, "Restarting connection to buildsys server\n");
	return true;
}

static bool client_callback_server(char *addr, int port)
{
	// connect to server:port
	buildsys_conn = us_net_connection_port_addr(addr, port, (us_net_connection_type){ true , false }, buildsys_server_dropped, buildsys_server_message, true, es);
	us_core_shutdown();
	if(buildsys_conn == NULL) us_core_restart();
	return true;
}

static bool client_callback_server_start(char *source, size_t len)
{
	// hrm, we dont know how ...
	fprintf(stderr, "Unable to start our own buildcont server\n");
	return true;
}

static void* update_thread(us_thread *t)
{
	while(1)
	{
		buildsys_update_metric();
		sleep(1);
	}
}

void buildsys::sendTarget(const char *name)
{
	if(buildsys_conn != NULL)
	{
		us_datacoding_set *ds = us_datacoding_set_create();
		us_datacoding_add_string(ds,'T',name);
		us_string_length sl = us_datacoding_string(ds);
		us_datacoding_set_destroy(ds);
		us_net_send(buildsys_conn, sl, 0x11111111);
		free(sl.string);
	}
}

#endif

void buildsys::log(const char *package, const char *fmt, ...)
{
	char *message = NULL;
	va_list args;
	va_start(args, fmt);
	vasprintf(&message, fmt, args);
	va_end(args);

	fprintf(stderr, "%s: %s\n", package, message);
#ifdef UNDERSCORE
	if(buildsys_conn != NULL)
	{
		us_datacoding_set *ds = us_datacoding_set_create();
		us_datacoding_add_string(ds,'P',package);
		us_datacoding_add_string(ds,'M',message);
		us_string_length sl = us_datacoding_string(ds);
		us_datacoding_set_destroy(ds);
		us_net_send(buildsys_conn, sl, 0x01010101);
		free(sl.string);
	}
#endif
	free(message);
}

int main(int argc, char *argv[])
{
	struct timespec start, end;
	
	clock_gettime(CLOCK_REALTIME, &start);

	log((char *)"BuildSys",(char *)"Buildsys (C++ version)");
	log((char *)"BuildSys", "Built: %s %s", __TIME__, __DATE__);

#ifdef UNDERSCORE
	log((char *)"BuildSys", "Underscore support is enabled, performance impact unknown");

	jq = us_jq_create(5);
	es = us_event_set_create(jq);
	ds = us_delay_set_create(jq);

	if(!us_client_core_connect(NULL, (us_net_connection_type){ true , false }, (char *)"buildsys", es, ds, NULL, NULL, NULL, client_callback_server, client_callback_server_start))
		exit(EXIT_FAILURE);
	
	us_thread_create(update_thread, 0, NULL);

	// Let it connect before we start sending messages at it
	sleep(5);
#endif
	
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
	int a = 2;
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
	
	log(argv[1], (char *)"Total time: %ds and %dms", (end.tv_sec - start.tv_sec) , (end.tv_nsec - start.tv_nsec) / 1000);
	
	return 0;
}
