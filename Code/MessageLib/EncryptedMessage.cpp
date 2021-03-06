#include <MessageLib/EncryptedMessage.hpp>

namespace msglib {
    EncryptedMessage::EncryptedMessage() {
        messageId = 4;
        message = "UNDEFINED";
    }

    EncryptedMessage::EncryptedMessage(std::string requestString, std::string messageName) {
        messageId = 4;
        message = requestString;
        name = messageName;
    }

    void EncryptedMessage::Pack(unsigned char* buffer) {
        int bufferPos = 0;
        
        bufferPos = IMessage::InsertInt(messageId, buffer, bufferPos);
        bufferPos = IMessage::InsertString(message, buffer, bufferPos);
        IMessage::InsertString(name, buffer, bufferPos);
    }

    void EncryptedMessage::Unpack(unsigned char* buffer) {
        int bufferPos = 0;

        bufferPos = IMessage::RetrieveInt(&messageId, buffer, bufferPos);
        bufferPos = IMessage::RetrieveString(&message, buffer, bufferPos);
        IMessage::RetrieveString(&name, buffer, bufferPos);
    }
}