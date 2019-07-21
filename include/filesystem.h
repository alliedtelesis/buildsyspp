#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

namespace buildsys {
	namespace filesystem {
		void create_directories(const std::string & path);
		void remove_all(const std::string & path);
		bool exists(const std::string & path);
		bool is_directory(const std::string & path);
	}
}

#endif /* _FILESYSTEM_H */
