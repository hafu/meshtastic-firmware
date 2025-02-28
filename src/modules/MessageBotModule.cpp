#include "MessageBotModule.h"
#include "DebugConfiguration.h"
#include "MeshModule.h"
#include "MeshService.h"
#include "MeshTypes.h"
#include "NodeDB.h"
#include "RadioInterface.h"
#include "configuration.h"
#include "main.h"
#include "meshtastic/mesh.pb.h"
#include "modules/NodeInfoModule.h"
#include <cstddef>


#define MSGBOTMOD_MAX_SIGNAL_STRING_SIZE 50
#define MSGBOTMOD_MAX_HOPS_STRING_SIZE 20

ProcessMessage MessageBotModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    auto &p = mp.decoded;
    LOG_INFO("MessageBotModule: from=0x%0x, to=0x%0x, channel=%u, rx_time=0x%0x, rx_snr=%f, hop_limit=%u, rx_rssi=%i, hop_start=%u, id=0x%x, msg=%.*s", mp.from, mp.to, mp.channel, mp.rx_time, mp.rx_snr, mp.hop_limit, mp.rx_rssi, mp.hop_start, mp.id, p.payload.size, p.payload.bytes);

    if (isToUs(&mp) && (startsWith(p.payload.bytes, "help") || startsWith(p.payload.bytes, "hilfe"))) {
        sendHelpReplyMessage(mp);
    } else if (startsWith(p.payload.bytes, "ping")) {
        handlePingMessage(mp);
    } else if (startsWith(p.payload.bytes, "test")) {
        handleTestMessage(mp);
    }

    return ProcessMessage::CONTINUE;
}

bool MessageBotModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

bool MessageBotModule::startsWith(const unsigned char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str++) != tolower((unsigned char)*prefix++)) {
            return false;
        }
    }
    return true;
}

void MessageBotModule::handlePingMessage(const meshtastic_MeshPacket &mp) {
    sendReplyMessage(mp, "pong 🏓");
}

void MessageBotModule::handleTestMessage(const meshtastic_MeshPacket &mp) {
    sendReplyMessage(mp, "check ✅");
}

void MessageBotModule::sendReplyMessage(const meshtastic_MeshPacket &mp, const char *replyMessage) {
    meshtastic_MeshPacket *p = allocAndConfReplyPacket(mp);
    if (p == nullptr) {
        return;
    }

    p->decoded.want_response = false;
    // TODO: set priority?
    p->priority = meshtastic_MeshPacket_Priority_DEFAULT;
    // p->priority = mp.priority;
    // NOTE: may leave unset?
    // https://github.com/meshtastic/firmware/blob/7d8e0ede6ccb4c621c70589834562276cb128687/src/mesh/generated/meshtastic/mesh.pb.h#L706
    p->want_ack = false;

    // prefix for message
    const static char *messagePrefix = "🤖";
    // buffer for the radio stats
    static char signalString[MSGBOTMOD_MAX_SIGNAL_STRING_SIZE];
    size_t signalStringSize = sizeof(signalString);
    // buffer for the hops text
    static char hopsString[MSGBOTMOD_MAX_HOPS_STRING_SIZE];
    size_t hopsStringSize = sizeof(hopsString);
    // buffer for the reply message
    static char replyString[MAX_LORA_PAYLOAD_LEN+1];
    size_t replyStringSize = sizeof(replyString);
    int nchars = 0;

    // assemble text for radio stats
    nchars = snprintf(
                      signalString,
                      signalStringSize,
                      "SNR %.2fdB RSSI %idBm",
                      mp.rx_snr, mp.rx_rssi
    );
    if (nchars <= 0 || nchars > signalStringSize) {
        LOG_ERROR("MessageBotModule: Failed to assemble signal string size: %d vs %d", signalStringSize, nchars);
        return;
    }

    // assemble hop text
    if ((mp.hop_start - mp.hop_limit) > 0) {
        nchars = snprintf(
            hopsString,
            hopsStringSize,
            "via %u hops",
            (mp.hop_start-mp.hop_limit)
        );
    } else if ((mp.hop_start - mp.hop_limit) == 0) {
        // could also us strncpy
        nchars = snprintf(
            hopsString,
            hopsStringSize,
            "%s", "direct"
        );
    } else {
        nchars = snprintf(
            hopsString,
            hopsStringSize,
            "%s", "via ? hops"
        );
        LOG_WARN("MessageBotModule: Strange: hop_start: %u, hop_limit: %u", mp.hop_start, mp.hop_limit);
    }
    if (nchars <= 0 || nchars > signalStringSize) {
        LOG_ERROR("MessageBotModule: Failed to assemble hops string size: %d vs %d", hopsStringSize, nchars);
        return;
    }

    // private message
    if (isToUs(&mp)) {
        nchars = snprintf(
            replyString,
            replyStringSize,
            "%s %s\n%s %s",
            messagePrefix, replyMessage, signalString, hopsString
        );
    // broadcast (channel) message
    } else if (!isFromUs(&mp) && mp.to == NODENUM_BROADCAST) {
        p->to = NODENUM_BROADCAST;
        meshtastic_NodeInfoLite *n = nodeDB->getMeshNode(getFrom(&mp));
        if (n != NULL) {
            nchars = snprintf(
                replyString,
                replyStringSize,
                "%s @%s %s\n%s %s",
                messagePrefix, n->user.short_name, replyMessage, signalString, hopsString
            );
        } else {
            LOG_WARN("MessageBotModule: Node (0x%0x) not in NodeDB", mp.from);
            nchars = snprintf(
                replyString,
                replyStringSize,
                "%s @!%0x %s\n%s %s",
                messagePrefix, mp.from, replyMessage, signalString, hopsString
            );
        }
    // handle edge cases
    } else {
        LOG_WARN("MessageBotModule: Unhandled message. Ignoring");
        return;
    }

    // send the message
    if (nchars > 0 && nchars <= replyStringSize) {
        sendReplyMessage(p, replyString);
    } else {
        LOG_ERROR("MessageBotModule: Failed to assemble reply string size: %d vs %d", replyStringSize, nchars);
    }
}

void MessageBotModule::sendHelpReplyMessage(const meshtastic_MeshPacket &mp) {
    meshtastic_MeshPacket *p = allocAndConfReplyPacket(mp);
    if (p == nullptr) {
        return;
    }
    const char *helpText =
        "I am Bot 🤖 running as module directly in the Meshtastic firmware. I "
        "react on messages starting with \"ping\" or \"test\". No human will "
        "read direct messages delivered to me.";

    sendReplyMessage(p, helpText);
}

meshtastic_MeshPacket *MessageBotModule::allocAndConfReplyPacket(const meshtastic_MeshPacket &mp) {
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        LOG_ERROR("MessageBotModule: Failed to allocate meshtastic_MeshPacket!");
        return nullptr;
    }

    // back to sender with same parameters
    p->to = mp.from;
    p->decoded.want_response = mp.decoded.want_response;
    p->channel = mp.channel;
    p->priority = mp.priority;
    p->want_ack = mp.want_ack;
    return p;
}

void MessageBotModule::sendReplyMessage(meshtastic_MeshPacket *p, const char *payload) {
    if (p == nullptr) {
        LOG_ERROR("MessageBotModule: Null packet pointer");
        return;
    }
    if (payload == nullptr) {
        LOG_ERROR("MessageBotModule: Null payload pointer");
        return;
    }

    p->decoded.payload.size = strlen(payload);
    memcpy(p->decoded.payload.bytes, payload, p->decoded.payload.size);
    if (airTime->isTxAllowedChannelUtil(true)) {
        service->sendToMesh(p);
        LOG_INFO("MessageBotModule: reply send");
    } else {
        LOG_WARN("MessageBotModule: can not send, air time exceeded");
    }
}
