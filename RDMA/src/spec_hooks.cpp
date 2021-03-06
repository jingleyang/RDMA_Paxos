#include <string>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include "include/consensus/consensus.h"
#include "include/proxy/proxy.h"
#include "include/output/output.h"

#define dprintf(fmt...)

struct proxy_node_t* proxy;

typedef int (*main_type)(int, char**, char**);

struct arg_type
{
	char **argv;
	int (*main_func) (int, char **, char **);
};

main_type saved_init_func = NULL;
void tern_init_func(int argc, char **argv, char **env)
{
	dprintf("%04d: __tern_init_func() called.\n", (int) pthread_self());
	if(saved_init_func)
		saved_init_func(argc, argv, env);

	printf("tern_init_func is called\n");
	char *config_path = "/home/hkucs/Documents/RDMA/target/nodes.local.cfg";

	char* log_dir = NULL;
	const char* id = getenv("node_id");
	uint32_t node_id = atoi(id);
	proxy = NULL;
	proxy = proxy_init(node_id, config_path, log_dir);

	pthread_t rep_th;
	pthread_create(&rep_th, NULL, &handle_accept_req, (void*)(proxy->con_node->consensus_comp));
}

typedef void (*fini_type)(void*);
fini_type saved_fini_func = NULL;

extern "C" int my_main(int argc, char **pt, char **aa)
{
	int ret;
	arg_type *args = (arg_type*)pt;
	dprintf("%04d: __libc_start_main() called.\n", (int) pthread_self());
	ret = args->main_func(argc, args->argv, aa);
	return ret;
}

extern "C" int __libc_start_main(
	void *func_ptr,
	int argc,
	char* argv[],
	void (*init_func)(void),
	void (*fini_func)(void),
	void (*rtld_fini_func)(void),
	void *stack_end)
{
	typedef void (*fnptr_type)(void);
	typedef int (*orig_func_type)(void *, int, char *[], fnptr_type,
		fnptr_type, fnptr_type, void*);
	orig_func_type orig_func;
	arg_type args;

	void * handle;
	int ret;

	// Get lib path.
	Dl_info dli;
	dladdr((void *)dlsym, &dli);
	std::string libPath = dli.dli_fname;
	libPath = dli.dli_fname;
	size_t lastSlash = libPath.find_last_of("/");
	libPath = libPath.substr(0, lastSlash);
	libPath += "/libc.so.6";
	libPath = "/lib/x86_64-linux-gnu/libc.so.6";
	if(!(handle=dlopen(libPath.c_str(), RTLD_LAZY))) {
		puts("dlopen error");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "__libc_start_main");

	if(dlerror()) {
		puts("dlerror");
		abort();
	}

	dlclose(handle);

	dprintf("%04d: __libc_start_main is hooked.\n", (int) pthread_self());

	args.argv = argv;
	args.main_func = (main_type)func_ptr;
	saved_init_func = (main_type)init_func;

	saved_fini_func = (fini_type)rtld_fini_func;

	char* target = "redis";
	if (NULL != strstr(argv[0], target))
	{
		ret = orig_func((void*)my_main, argc, (char**)(&args), (fnptr_type)tern_init_func, (fnptr_type)fini_func, rtld_fini_func, stack_end);
	}else{
		ret = orig_func((void*)my_main, argc, (char**)(&args), (fnptr_type)saved_init_func, (fnptr_type)fini_func, rtld_fini_func, stack_end);
	}

	return ret;

}

extern "C" ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
	typedef ssize_t (*orig_recv_type)(int, void *, size_t, int);
	orig_recv_type orig_recv;
	orig_recv = (orig_recv_type) dlsym(RTLD_NEXT, "recv");
	ssize_t ret = orig_recv(sockfd, buf, len, flags);

	if (proxy != NULL && proxy->con_node->zfd != sockfd && proxy->con_node->cur_view.leader_id == proxy->con_node->node_id)
	{
		rsm_op(proxy->con_node->consensus_comp, buf, ret, CSM);
	}

	return ret;
}

extern "C" ssize_t read(int fd, void *buf, size_t count)
{
	typedef ssize_t (*orig_read_type)(int, void *, size_t);
	orig_read_type orig_read;
	orig_read = (orig_read_type) dlsym(RTLD_NEXT, "read");
	ssize_t ret = orig_read(fd, buf, count);

	struct stat sb;
	fstat(fd, &sb);

	if (ret > 0 && (sb.st_mode & S_IFMT) == S_IFSOCK && proxy != NULL && proxy->con_node->zfd != fd && proxy->con_node->cur_view.leader_id == proxy->con_node->node_id)
	{
		rsm_op(proxy->con_node->consensus_comp, buf, ret, CSM);
	}

	return ret;
}

extern "C" ssize_t write(int fd, const void *buf, size_t count)
{
	typedef ssize_t (*orig_write_type)(int, const void *, size_t);
	orig_write_type orig_write;
	orig_write = (orig_write_type) dlsym(RTLD_NEXT, "write");
	ssize_t ret = orig_write(fd, buf, count);

	struct stat sb;
	fstat(fd, &sb);

	if (ret > 0 && (sb.st_mode & S_IFMT) == S_IFSOCK && proxy != NULL && proxy->con_node->zfd != fd)
	{
		pthread_mutex_lock(&proxy->con_node->consensus_comp->output_handler->lock);
		store_output(buf, ret, proxy->con_node->consensus_comp->output_handler);
		long output_idx = proxy->con_node->consensus_comp->output_handler->count;
		pthread_mutex_unlock(&proxy->con_node->consensus_comp->output_handler->lock);
		if (proxy->con_node->cur_view.leader_id == proxy->con_node->node_id && output_idx % CHECK_PERIOD == 0)
		{
			rsm_op(proxy->con_node->consensus_comp, &output_idx, sizeof(long), CHECK);
		}
	}

	return ret;
}