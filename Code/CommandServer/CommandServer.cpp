#include <CommandServer/CommandServer.hpp>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <CommandServer/ClientInfo.hpp>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <thread>
#include <atomic>

// Message lib
#include <MessageLib/IMessage.hpp>
#include <MessageLib/ResponseMessage.hpp>
#include <MessageLib/StartTransMessage.hpp>
#include <MessageLib/RequestMessage.hpp>
#include <MessageLib/EncryptedMessage.hpp>

// Sec lib
#include <SecurityLib/SecurityService.hpp>
#include <SecurityLib/Structures/SecurityConfiguration.hpp>
#include <SecurityLib/ConfigurationGenerator.hpp>

#include <iostream>

using namespace std;

CommandServer::CommandServer() {
    // Default constructor
    // Create the security service.
    // Make default config
    securitylib::ConfigurationGenerator config_generator;
    securitylib::SecurityConfiguration config = config_generator.GenerateDefaultConfiguration();

    sec_service = make_shared<securitylib::SecurityService>(config);

    sec_service->symmetricKeyGenerationService->GenerateKeys(private_key, public_key);
    sec_service->keyExchangeService->GenerateIntermediateKeys(KE_private_key, KE_public_key);

    _is_running.fetch_add(1, memory_order_seq_cst);
}

void CommandServer::HandleClientConnection(int socket) {
    int const bufferLen = 4096;
    char buffer[bufferLen];

    while (_is_running.load() != 0) {
        memset(buffer, 0, 4096);

        // Wait for client to send data.
        int bytesReceived = recv(socket, buffer, bufferLen, 0);

        if (bytesReceived == 0) {
            cout << "Client disconnected." << endl;
            break;
        }

        if (_is_running.load() < 1) {
            break;
        }

        int messageId;
        msglib::IMessage::RetrieveInt(&messageId, (unsigned char*)buffer, 0);

        HandleMessage(messageId, buffer);

        send(socket, buffer, bufferLen, 0);
    }

    close(socket);
}

void CommandServer::HandleMessage(int messageId, char* buffer) {
    switch (messageId) {
        case 1: {// Start transaction message

            // Take the name from the start trans msg
            msglib::StartTransMessage incoming_msg;
            incoming_msg.Unpack((unsigned char*)buffer);

            ClientInfo ci;
            ci.KE_key = "";

            {
                std::unique_lock lock(client_info_mutex);
                client_info[incoming_msg.name] = ci;
            }
            

            // Need to send back the public key
            msglib::ResponseMessage response("StartTrans", public_key);
            response.Pack((unsigned char*)buffer);
            
            }
            break;

        case 2: {// Request
            msglib::RequestMessage incoming_msg;
            incoming_msg.Unpack((unsigned char*)buffer);

            std::string request = incoming_msg.request.substr(0, 10);
            if (request == "TradeKeys ") {
                // Get the key out of it
                std::string client_public_key = incoming_msg.request.substr(10);
                // Perform the key exchange

                std::string key = sec_service->keyExchangeService->GenerateFinalKey(KE_private_key, client_public_key);

                // Store the key
                {
                    std::unique_lock lock(client_info_mutex);
                    client_info[incoming_msg.name].KE_key = key;
                }

                // Craft and Send the response.
                msglib::ResponseMessage response("TradeKeys", KE_public_key);

                response.Pack((unsigned char*)buffer);
            }

            }
            break;

        case 4: { // Encrypted Message
            msglib::EncryptedMessage incoming_msg;
            incoming_msg.Unpack((unsigned char*)buffer);

            // Get the key from the store
            std::string key;
            {
                std::shared_lock lock_shared(client_info_mutex); // Lock for read-only
                key.assign(client_info[incoming_msg.name].KE_key);
            }

            std::string decrypted_message = sec_service->encryptionService->DecryptData(key, incoming_msg.name, incoming_msg.message);

            // Get the message type
            std::string message_request = decrypted_message.substr(0, 10);

            // Switch on the message_request
            if (message_request == "StatusUpd ") {
                // Params in one string
                std::string params = decrypted_message.substr(10);
                
                // Extract the params
                vector<string> params_vec;
                SplitString(params, ',', params_vec);

                // Map the params
                {
                    unique_lock lock(_client_statuses_mutex);
                    
                    for (int i = 0; i < params_vec.size(); i++) {
                        vector<string> single_param;
                        SplitString(params_vec[i], '=', single_param);
                        _client_statuses[incoming_msg.name][single_param[0]] = single_param[1];

                    }
                }
                

            }
            // Encrypt and send back

            std::string encrypted_message = sec_service->encryptionService->EncryptData(key, incoming_msg.name, decrypted_message);
            msglib::EncryptedMessage response(encrypted_message, "Server");

            response.Pack((unsigned char*)buffer);

            }
            break;

        default:
            break;
    }
}

// Split string method from https://www.oreilly.com/library/view/c-cookbook/0596007612/ch04s07.html
void CommandServer::SplitString(const string& s, char c, vector<string>& v) {
    string::size_type i = 0;
    string::size_type j = s.find(c);

    while (j != string::npos) {
        v.push_back(s.substr(i, j-i));
        i = ++j;
        j = s.find(c, j);

        if (j == string::npos)
            v.push_back(s.substr(i, s.length()));
    }
}

vector<string> CommandServer::GetClientNames() {
    vector<string> client_names;

    {
        unique_lock lock(_client_statuses_mutex);

        for(pair<string, map<string, string>> element : _client_statuses) {
                client_names.push_back(element.first);
        }

    }

    return client_names;
}

vector<string> CommandServer::GetClientStatus(string client_name) {
    vector<string> client_status;

    map<string, string> statuses;
    {
        unique_lock lock(_client_statuses_mutex);

        statuses = _client_statuses[client_name];
    }

    for(pair<string, string> element : statuses) {
                client_status.push_back(element.first + " : " + element.second);
    }

    return client_status;

}

void CommandServer::CommandThreadMain() {
    while(_is_running.load() > 0) {
        cout << "Please enter your command: ";

        string input;

        cin >> input;

        string command = input.substr(0, 4);

        if (command == "list") {
            vector<string> names = GetClientNames();
            for (string name : names) {
                cout << " -" << name << endl;
            }
        }
        else if (command == "show") {
            string name;
            cin >> name;
            vector<string> statuses = GetClientStatus(name);

            cout << "Showing status of " << name << endl;

            for (string status : statuses) {
                cout << " -" << status << endl;
            }
        }
        else if (command == "quit") {
            StopServer();
        }
        else {
            cout << "Not a valid command. Valid commands are 'list' and 'show'." << endl;
        }
    }
}

thread CommandServer::RunCommandThread() {
    thread command_thread(&CommandServer::CommandThreadMain, this);

    return command_thread;
}

bool CommandServer::IsRunning() {
    return _is_running.load() > 0;
}

void CommandServer::StopServer() {
    _is_running.fetch_and(0, memory_order_seq_cst);
}