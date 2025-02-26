#pragma once
#include "SinglePortModule.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/portnums.pb.h"

/**
 * A simple example module that just replies with "Message received" to any message it receives.
 */
class MessageBotModule : public SinglePortModule
{
  public:
    /** Constructor
     * name is for debugging output
     */
    MessageBotModule() : SinglePortModule("MessageBotModule", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

  protected:
    /** For reply module we do all of our processing in the (normally optional)
     * want_replies handling
     */
    // virtual meshtastic_MeshPacket *allocReply() override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    int startsWith(const unsigned char *str, const char *prefix);
    void handlePingMessage(const meshtastic_MeshPacket &mp);
    void handleTestMessage(const meshtastic_MeshPacket &mp);
    void sendReplyMessage(const meshtastic_MeshPacket &mp, const char *replyMessage);
};
