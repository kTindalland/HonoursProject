#ifndef __IFILEIO
#define __IFILEIO

#include <string>
#include <SecurityLib/Structures/SecurityConfiguration.hpp>

namespace securitylib {
	class IFileIO {
		public:
			virtual SecurityConfiguration ReadConfiguration(std::string filepath) = 0;
			virtual void WriteConfiguration(SecurityConfiguration config, std::string filepath) = 0;
			virtual ~IFileIO() { };
	};
}

#endif
