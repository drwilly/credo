#include <skalibs/stddjb.h>
#include <skalibs/buffer.h>

#include <unistd.h>
#include <fcntl.h>

#include "reporting.h"
#include "path.h"

#include "checksum.h"

#include "prereqs.h"

static const char *dbfile_suffix = ".prereqs";

int
prereqs_existfor(const char *target) {
	char dbfile[str_len(target) + str_len(dbfile_suffix) + 1];
	str_copy(dbfile, target);
	str_copy(dbfile + str_len(target), dbfile_suffix);
	return path_exists(dbfile);
}

int
prereqs_renamefor(const char *target, const char *tmpfile) {
	char dbfile[str_len(target) + str_len(dbfile_suffix) + 1];
	str_copy(dbfile, target);
	str_copy(dbfile + str_len(target), dbfile_suffix);
	return rename(tmpfile, dbfile);
}

static
int
prereq_created(const char *file) {
	return path_exists(file);
}

static
int
prereq_changed_source(const char *file, const char *checksum_str) {
	return !path_exists(file) || file_checksum_changed(file, checksum_str);
}

static
int
prereq_changed_target(const char *file, const char *checksum_str) {
	return !path_exists(file) || file_checksum_changed(file, checksum_str) || prereqs_changedfor(file);
}

static
int
prereqs_changed(int db_fd) {
	char buf[BUFFER_INSIZE];
	buffer b = BUFFER_INIT(&buffer_read, db_fd, buf, BUFFER_INSIZE);

	stralloc ln = STRALLOC_ZERO;
	int changed = 0;
	int r;
	while(!changed && (r = skagetln(&b, &ln, '\n')) > 0) {
		char *dep[3]; // FIXME
		int i;
		dep[0] = ln.s;
		i = str_chr(dep[0], '\t');
		dep[0][i] = '\0';
		dep[1] = dep[0] + i + 1;
		i = str_chr(dep[1], '\t');
		dep[1][i] = '\0';
		dep[2] = dep[1] + i + 1;
		ln.s[ln.len-1] = '\0';
		switch(ln.s[0]) {
		case 't':
			if(str_diff(dep[0], "target")) die("" /* TODO */);
			changed = prereq_changed_target(dep[1], dep[2]);
			break;
		case 's':
			if(str_diff(dep[0], "source")) die("" /* TODO */);
			changed = prereq_changed_source(dep[1], dep[2]);
			break;
		case 'a':
			if(str_diff(dep[0], "absent")) die("" /* TODO */);
			changed = prereq_created(dep[1]);
			break;
		default:
			die("" /* TODO */);
		}
		ln.len = 0;
	}
	stralloc_free(&ln);

	if(r == -1) die("");

	return changed;
}

static
int
prereqs_opencheckclose(const char *file) {
	int fd = open_read(file);
	if(fd == -1) {
		die_errno("open_read('%s') failed", file);
	}
	int changed = prereqs_changed(fd);
	fd_close(fd);

	return changed;
}

int
prereqs_changedfor(const char *target) {
	char dbfile[str_len(target) + str_len(dbfile_suffix) + 1];
	str_copy(dbfile, target);
	str_copy(dbfile + str_len(target), dbfile_suffix);
	return prereqs_opencheckclose(dbfile);
}

size_t
prereq_record_target(const char *file) {
	char checksum_str[20*2+1];
	if(path_exists(file)) {
		unsigned char checksum[20];
		file_checksum_compute(file, checksum);
		hexstring_from_checksum(checksum_str, checksum);
	} else {
		// virtual target
		str_copy(checksum_str, "0000000000000000000000000000000000000000");
	}

	struct iovec iov[] = {
		{
			.iov_base = "target",
			.iov_len = 6,
		}, {
			.iov_base = "\t",
			.iov_len = 1,
		}, {
			.iov_base = file,
			.iov_len = str_len(file),
		}, {
			.iov_base = "\t",
			.iov_len = 1,
		}, {
			.iov_base = checksum_str,
			.iov_len = 20*2,
		}, {
			.iov_base = "\n",
			.iov_len = 1,
		}
	};

	size_t total = 0;
	for(int i = 0; i < 6; i++) {
		total += iov[i].iov_len;
	}
	size_t w = fd_writev(3, iov, 6);
	return (w == -1) ? w : total - w;
}

size_t
prereq_record_source(const char *file) {
	unsigned char checksum[20];
	char checksum_str[20*2+1];
	file_checksum_compute(file, checksum);
	hexstring_from_checksum(checksum_str, checksum);

	struct iovec iov[] = {
		{
			.iov_base = "source",
			.iov_len = 6,
		}, {
			.iov_base = "\t",
			.iov_len = 1,
		}, {
			.iov_base = file,
			.iov_len = str_len(file),
		}, {
			.iov_base = "\t",
			.iov_len = 1,
		}, {
			.iov_base = checksum_str,
			.iov_len = 20*2,
		}, {
			.iov_base = "\n",
			.iov_len = 1,
		}
	};

	size_t total = 0;
	for(int i = 0; i < 6; i++) {
		total += iov[i].iov_len;
	}
	size_t w = fd_writev(3, iov, 6);
	return (w == -1) ? w : total - w;
}

size_t
prereq_record_absent(const char *file) {
	struct iovec iov[] = {
		{
			.iov_base = "absent",
			.iov_len = 6,
		}, {
			.iov_base = "\t",
			.iov_len = 1,
		}, {
			.iov_base = file,
			.iov_len = str_len(file),
		}, {
			.iov_base = "\n",
			.iov_len = 1,
		}
	};

	size_t total = 0;
	for(int i = 0; i < 4; i++) {
		total += iov[i].iov_len;
	}
	size_t w = fd_writev(3, iov, 4);
	return (w == -1) ? w : total - w;
}