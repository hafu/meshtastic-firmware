#pragma once
#include "SinglePortModule.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/portnums.pb.h"

/**
 * A simple bot which replies to certain messages with a response.
 */
class MessageBotModule : public SinglePortModule
{
  public:
    /** Constructor
     * name is for debugging output
     */
    MessageBotModule() : SinglePortModule("MessageBotModule", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

    struct StatsStruct {
     uint32_t privatePingMessages;
     uint32_t privateTestMessages;
     uint32_t channelPingMessages;
     uint32_t channelTestMessages;
     uint32_t repliesSend;
     uint32_t airTimeExceeded;

     StatsStruct():
       privatePingMessages(0),
       privateTestMessages(0),
       channelPingMessages(0),
       channelTestMessages(0),
       repliesSend(0),
       airTimeExceeded(0) {}
    };
  protected:
    /** Called to handle a particular incoming message
    @return ProcessMessage::STOP if you've guaranteed you've handled this message and no other handlers should be considered for
    it
    */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

    /**
     * @return true if you want to receive the specified portnum
     */    
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;

    /** Check if *str starts with *prefix, case insensitive
     * @return true if *str starts with *prefix (case insensitive)
     */
    bool startsWith(const unsigned char *str, const char *prefix);

    /** 
     * Wrapper around sendReplyMessage
     */
    void handlePingMessage(const meshtastic_MeshPacket &mp);

    /**
     * Wrapper around sendReplyMessage
     */
    void handleTestMessage(const meshtastic_MeshPacket &mp);

    /**
     * Send a basic reply message
     */
    void sendReplyMessage(const meshtastic_MeshPacket &mp, const char *replyMessage);

    /**
     * Send help reply message
     */
    void sendHelpReplyMessage(const meshtastic_MeshPacket &mp);

    void sendStatsMessage(const meshtastic_MeshPacket &mp);


    /** Allocate and configur mesh packet with sane defaults
     * @return new meshtastic_MeshPacket based provided packet 
     */
    meshtastic_MeshPacket *allocAndConfReplyPacket(const meshtastic_MeshPacket &mp);

    /**
     * Send reply message with packet p and payload into mesh.
     */
    void sendReplyMessage(meshtastic_MeshPacket *p, const char *payload);
 private:
  StatsStruct stats;
};
