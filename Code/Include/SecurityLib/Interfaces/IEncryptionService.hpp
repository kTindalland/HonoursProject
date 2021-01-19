#include <string>

namespace securitylib {

	class IEncryptionService {
	
		public:
			virtual std::string EncryptData(std::string key, std::string data) = 0;
			virtual std::string DecryptData(std::string key, std::string data) = 0;

	};

}
