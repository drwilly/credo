#include <skalibs/djbunix.h>

#include "reporting.h"
#include "path.h"
#include "predeps.h"

#include "options.h"

static
int
args_filter_unchanged(int argc, char *argv[]) {
	int c = 1;
	for(int i = 1; i < argc; i++) {
		if(!predeps_existfor(argv[i])) {
			if(path_exists(argv[i])) {
				// not a build target
				predep_record_source(argv[i]);
			} else {
				// target (clean build)
				argv[c++] = argv[i];
			}
		} else if(predeps_changedfor(argv[i])) {
			// target (deps changed)
			argv[c++] = argv[i];
		} else {
			// target (deps unchanged)
			predep_record_target(argv[i]);
		}
	}

	return c;
}

int
main(int argc, char *argv[]) {
	fd_ensure_open(3, 1);
	coe(3);

	argc = args_filter_options(argc, argv);
	argc = args_filter_unchanged(argc, argv);

	if(argc <= 1) {
		return 0;
	}

	pid_t pid = fork();
	if(pid == -1) {
		die_errno("fork() failed");
	} else if(!pid) {
		argv[0] = "redo";
		execvp(argv[0], argv);
		die_errno("exec('%s', ...) failed", argv[0]);
	} else {
		int status;
		waitpid_nointr(pid, &status, 0);
		if(WEXITSTATUS(status) == 0) {
			for(int i = 1; i < argc; i++) {
				predep_record_target(argv[i]);
			}
		}
		fd_close(3);
		return WEXITSTATUS(status);
	}
}
