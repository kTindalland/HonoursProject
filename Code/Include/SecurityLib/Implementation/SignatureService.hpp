#ifndef __SIGSERV
#define __SIGSERV

#include <string>
#include <SecurityLib/Interfaces/ISignatureService.hpp>

namespace securitylib {
	class SignatureService: public ISignatureService {
		std::string SignData(std::string data, std::string private_key);
		bool CheckSignature(std::string signed_data, std::string public_key, std::string check_hash);
	};
}

#endif
