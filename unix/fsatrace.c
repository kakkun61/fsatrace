#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <libproc.h>
#include "fsatrace.h"

extern char   **environ;

static void
dump(const char *path, char *p)
{
	int		fd;
	ssize_t		r;
	size_t		sz;
	if (strcmp(path, "-"))
		fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	else
		fd = 1;
	if (fd < 0)
		fprintf(stderr, "Unable to open output file '%s'\n", path);
	sz = (size_t) * (unsigned *)p;
	r = write(fd, p + sizeof(unsigned), sz);
	assert(r == sz);
	if (fd != 1)
		close(fd);
}

static unsigned long
hash(unsigned char *str)
{
	unsigned long	h = 5381;
	int		c;
	while ((c = *str++))
		h = ((h << 5) + h) + c;
	return h;
}

static void
fatal(const char *fmt,...)
{
	va_list		ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	void           *buf;
	int		fd;
	int		r;
	char		so        [PATH_MAX];
	char		shname    [PATH_MAX];
	char		fullpath  [PATH_MAX];
	int		rc = EXIT_FAILURE;
	int		child;
	const char     *out;
	if (argc < 4 || strcmp(argv[2], "--"))
		fatal("Usage: %s <output> -- <cmdline>\n", argv[0]);
	out = argv[1];
	snprintf(shname, sizeof(shname), "/%ld", hash((unsigned char *)out));
	for (size_t i = 0, l = strlen(shname); i < l; i++)
		if (shname[i] == '/')
			shname[i] = '_';
	shm_unlink(shname);
	fd = shm_open(shname, O_CREAT | O_RDWR, 0666);
	r = ftruncate(fd, LOGSZ);
	assert(!r);
	buf = mmap(0, LOGSZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	proc_pidpath(getpid(), fullpath, sizeof(fullpath));
#ifdef __APPLE__
	snprintf(so, sizeof(so), "%s.dylib", fullpath);
	setenv("DYLD_INSERT_LIBRARIES", so, 1);
	setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
#else
	snprintf(so, sizeof(so), "%s.so", fullpath);
	setenv("LD_PRELOAD", so, 1);
#endif
	setenv(ENVOUT, shname, 1);
	child = fork();
	if (!child) {
		execvp(argv[3], argv + 3);
		fatal("Unable to execute command '%s'\n", argv[3]);
	}
	r = wait(&rc);
	assert(r >= 0);
	rc = WEXITSTATUS(rc);
	if (!rc || *out == '-')
		dump(out, buf);
	munmap(buf, LOGSZ);
	close(fd);
	shm_unlink(shname);
	return rc;
}
