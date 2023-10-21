#include <libmem/libmem.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

struct module_t {
	uintptr_t base;
	uintptr_t end;
	char path[PATH_MAX];
};

int enum_modules(int (*callback)(struct module_t *))
{
	DIR *d;
	struct dirent *dir;
	struct module_t mod;
	regex_t regex;
	regmatch_t matches[3];

	d = opendir("/proc/self/map_files");
	if (!d)
		return -1;

	if (regcomp(&regex, "([a-z0-9]+)-([a-z0-9]+)", REG_ICASE | REG_EXTENDED))
		goto CLOSE_RET;

	mod.base = 0;
	mod.end = 0;
	
	while ((dir = readdir(d)) != NULL) {
		if (!regexec(&regex, dir->d_name, 3, matches, 0)) {
			uintptr_t start = strtoul(&dir->d_name[matches[1].rm_so], NULL, 16);
			uintptr_t end = strtoul(&dir->d_name[matches[2].rm_so], NULL, 16);
			char path[PATH_MAX];
			char real_path[PATH_MAX];
			ssize_t result;

			snprintf(path, sizeof(path), "/proc/self/map_files/%s", dir->d_name);
			if ((result = readlink(path, real_path, sizeof(real_path))) == -1)
				continue;

			real_path[result] = '\0';
			
			if (!mod.base) {
				mod.base = start;
				mod.end = end;
				memcpy(mod.path, real_path, sizeof(mod.path));
			} else {
				if (start != mod.end || strcmp(mod.path, real_path)) {
					callback(&mod);
					mod.base = start;
					mod.end = end;
					memcpy(mod.path, real_path, sizeof(mod.path));
				} else {
					mod.end = end;
				}
			}
		}
	}

	if (mod.base)
		callback(&mod);

	regfree(&regex);

CLOSE_RET:
	closedir(d);
	
	return 0;
}

lm_bool_t lm_callback(lm_module_t *mod, lm_void_t *arg)
{
	printf("module: %s %zx-%zx\n", mod->path, mod->base, mod->end);
	return LM_TRUE;
}

int callback(struct module_t *mod)
{
	printf("module: %s %zx-%zx\n", mod->path, mod->base, mod->end);
	return 1;
}

int main()
{
	clock_t start;
	clock_t end;
	size_t i;

	printf("doing useless random allocations to fill /proc/self/maps\n");
	for (i = 0; i < 10000; ++i) {
		int prot = random() & (PROT_EXEC | PROT_READ | PROT_WRITE);

		void *_alloc = mmap(NULL, sysconf(_SC_PAGESIZE), prot, MAP_PRIVATE | MAP_ANON, -1, 0);
	}
	
	printf("testing map_files\n");

	printf("[*] enumerate modules with libmem\n");
	start = clock();
	LM_EnumModules(lm_callback, NULL);
	end = clock();
	printf("[*] finished - time: %lf\n", (double)(end - start) / CLOCKS_PER_SEC);

	printf("[*] enumerate modules with /proc/<pid>/map_files\n");
	start = clock();
	enum_modules(callback);
	end = clock();
	printf("[*] finished - time: %lf\n", (double)(end - start) / CLOCKS_PER_SEC);

	printf("[*] press enter to exit...\n");
	scanf("%*c");
	
	return 0;
}