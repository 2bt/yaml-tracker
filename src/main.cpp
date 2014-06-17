#include <iostream>
#include <sys/inotify.h>

#include "server.h"


using namespace std;


int main(int argc, char** argv) {


	if (argc != 2) {
		printf("usage: %s tune-file\n", argv[0]);
		return 0;
	}
	const char* filename = argv[1];


	YAML::Node node = YAML::LoadFile(filename);

	cout << "playing...\n";
	Server server;
	server.initialize(node);


	int fd = inotify_init();
	int wd = inotify_add_watch(fd, filename, IN_MODIFY);
	for (;;) {
		inotify_event event;
		if(read(fd, &event, sizeof(inotify_event)) > 0) {
			if (event.wd != wd) continue;

			inotify_rm_watch(fd, wd);
			wd = inotify_add_watch(fd, filename, IN_MODIFY);

			try {
				node = YAML::LoadFile(filename);
				server.initialize(node);
			}
			catch (const YAML::Exception& e) {
				cout << e.what() << endl;
			}
		}
	}

	inotify_rm_watch(fd, wd);
	close(fd);

	return 0;
}
