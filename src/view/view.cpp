#include "view.h"

namespace server {

View::View() {};

char* View::tpl(const char* file_name, unsigned int& content_length) {

	ssize_t file_name_length = strlen(file_name);

	short add = 0;

	if(file_name[0] != '/') {
		add++;
	}

	char* path = (char*)malloc(base_path_length + file_name_length + 1 + add);

	memcpy(path,                    base_path, base_path_length);

	if(add) {
		memcpy(path + base_path_length, "/", 1);
	}

	memcpy(path + add + base_path_length, file_name, file_name_length);

	path[base_path_length + file_name_length + add] = 0;

	int fd = open(path, O_RDONLY);

    free(path);

	if(fd == -1) {
		return nullptr;
	}

	content_length = lseek(fd, 0, SEEK_END);

	char* buf = (char*)malloc(content_length);

	lseek(fd, 0, SEEK_SET);

	read(fd, buf, content_length);

	close(fd);

    return buf;
};

View::~View() {};

} // namespace