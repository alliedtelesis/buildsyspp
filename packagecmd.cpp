#include <buildsys.h>

bool PackageCmd::Run(const char *package)
{
	if (this->skip)
		return true;

	char **ne = NULL;
	size_t ne_cnt = 0;
	if(this->envp != NULL)
	{
		// collect the current enviroment
		ne = (char **)calloc(1, sizeof(char *));
		size_t e = 0;
		while(environ[e] != NULL)
		{
			ne_cnt++;
			ne = (char **)realloc(ne, sizeof(char *) * (ne_cnt + 1));
			ne[ne_cnt-1] = strdup(environ[e]);
			ne[ne_cnt] = NULL;
			e++;
		} 
		// append any new enviroment to it
		for(e = 0; e < this->envp_count; e++)
		{
			ne_cnt++;
			ne = (char **)realloc(ne, sizeof(char *) * (ne_cnt + 1));
			ne[ne_cnt-1] = strdup(this->envp[e]);
			ne[ne_cnt] = NULL;
		}
	}
	bool res = true;
	if(run(package, this->args[0], this->args, this->path, ne) != 0) res = false;
	
	if(ne != NULL)
	{
		for(int i = 0; ne[i] != NULL; i++)
		{
			free(ne[i]);
		}
		free(ne);
	}
	return res;
}
