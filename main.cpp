#include <buildsys.h>

World *buildsys::WORLD;

#ifdef UNDERSCORE_MONITOR
static us_job_queue *jq;
static us_event_set *es;
static us_delay_set *ds;
us_net_connection *buildsys_conn;
us_queue_fifo *buildsys_send_queue = NULL;

struct bs_send_msg_s {
	uint32_t code;
	us_string_length sl;
};

static bool buildsys_server_message(us_net_message *m)
{
	fprintf(stderr, "Got message from buildsys server (ID = %x)\n", m->ID);
	us_datacoding_set *ds = us_datacoding_set_create_string(m->sl);
//	us_datacoding_data *d = us_datacoding_get(ds);
	switch(m->ID)
	{
		default:
			fprintf(stderr, "Unknown message from build controller\n");
	}
	us_datacoding_set_destroy(ds);
	return true;
}

void *send_buildsys_thread(us_thread *t)
{
	us_queue_fifo *f = (us_queue_fifo *)t->priv;
	while(1)
	{
		struct bs_send_msg_s *sm = (struct bs_send_msg_s *)us_fifo_get(f);
		if(sm == NULL)
		{
			sleep(1); continue;
		}
		us_net_send(buildsys_conn, sm->sl, sm->code);
		free(sm->sl.string);
		free(sm);
	}
	return NULL;
}

void send_buildsys_message(uint32_t code, us_string_length sl)
{
	struct bs_send_msg_s *msg = (struct bs_send_msg_s *)calloc(1, sizeof(struct bs_send_msg_s));
	msg->code = code;
	msg->sl = sl;
	us_fifo_put(buildsys_send_queue, msg);
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
		send_buildsys_message(0x10101010, sl);
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
	return NULL;
}

void buildsys::sendTarget(const char *name)
{
	us_datacoding_set *ds = us_datacoding_set_create();
	us_datacoding_add_string(ds,'T',name);
	us_string_length sl = us_datacoding_string(ds);
	us_datacoding_set_destroy(ds);
	send_buildsys_message(0x11111111, sl);
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
#ifdef UNDERSCORE_MONITOR
	us_datacoding_set *ds = us_datacoding_set_create();
	us_datacoding_add_string(ds,'P',package);
	us_datacoding_add_string(ds,'M',message);
	us_string_length sl = us_datacoding_string(ds);
	us_datacoding_set_destroy(ds);
	send_buildsys_message(0x01010101, sl);
#endif
	free(message);
}

#ifdef UNDERSCORE
void buildsys::program_output(const char *package, const char *mesg)
{
	fprintf(stdout, "%s: %s\n", package, mesg);
#ifdef UNDERSCORE_MONITOR
	us_datacoding_set *ds = us_datacoding_set_create();
	us_datacoding_add_string(ds,'P',package);
	us_datacoding_add_string(ds,'M',mesg);
	us_string_length sl = us_datacoding_string(ds);
	us_datacoding_set_destroy(ds);
	send_buildsys_message(0x01010102, sl);
#endif	
}
#endif

int main(int argc, char *argv[])
{
	struct timespec start, end;
	
	clock_gettime(CLOCK_REALTIME, &start);

	log((char *)"BuildSys",(char *)"Buildsys (C++ version)");
	log((char *)"BuildSys", "Built: %s %s", __TIME__, __DATE__);

#ifdef UNDERSCORE_MONITOR
	log((char *)"BuildSys", "Underscore support is enabled, performance impact unknown");

	jq = us_jq_create(5);
	es = us_event_set_create(jq);
	ds = us_delay_set_create(jq);
	
	if(!us_client_core_connect(NULL, (us_net_connection_type){ true , false }, (char *)"buildsys", es, ds, NULL, NULL, NULL, client_callback_server, client_callback_server_start))
		exit(EXIT_FAILURE);
	
	buildsys_send_queue = us_fifo_create();
	
	us_thread_create(update_thread, 0, NULL);
	us_thread_create(send_buildsys_thread, 0, buildsys_send_queue);

	// Let it connect before we start sending messages at it
	sleep(1);
#endif
	
	if(argc <= 1)
	{
		error((char *)"At least 1 parameter is required");
		exit(-1);
	}
	
	WORLD = new World();
#ifdef UNDERSCORE_MONITOR
	WORLD->setES(es);
#endif
	
	if(!interfaceSetup(WORLD->getLua()))
	{
		error((char *)"interfaceSetup: Failed");
		exit(-1);
	}
	
	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	int a = 2;
	bool foundDashDash = false;
	while(a < argc && !foundDashDash)
	{
		if(!strcmp(argv[a],"--clean"))
		{
			WORLD->setCleaning();
		} else
		if(!strcmp(argv[a],"--skip-configure"))
		{
			WORLD->setSkipConfigure();
		} else
#ifdef UNDERSCORE
		if(!strcmp(argv[a],"--no-output-prefix") || !strcmp(argv[a],"--nop"))
		{
			WORLD->clearOutputPrefix();
		} else
#else
		if(!strcmp(argv[a],"--no-output-prefix") || !strcmp(argv[a],"--nop"))
		{
			std::cerr << argv[0] << ": " << argv[a] << " is only supported with UNDERSCORE=y, ignoring" << std::endl;
		} else
#endif
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
		// then we can preload the feature set
		while(a < argc)
		{
			if(!WORLD->setFeature(argv[a]))
			{
				error("setFeature: Failed");
				exit(-1);
			}
			a++;
		}
	}
	if(!WORLD->basePackage(argv[1]))
	{
		error("Building: Failed");
		exit(-1);
	}

	// Write out the dependency graph
	WORLD->output_graph();

	clock_gettime(CLOCK_REALTIME, &end);
	
	if (end.tv_nsec >= start.tv_nsec)
	    log(argv[1], (char *)"Total time: %ds and %dms",
		(end.tv_sec - start.tv_sec),
		(end.tv_nsec - start.tv_nsec) / 1000000);
	else
	    log(argv[1], (char *)"Total time: %ds and %dms",
		(end.tv_sec - start.tv_sec - 1),
		(end.tv_nsec + start.tv_nsec) / 1000000);
	
	return 0;
}
