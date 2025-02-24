#include "ExperimentModule.h"
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


#define EXPMOD_MAX_SIGNAL_STRING_SIZE 50
#define EXPMOD_MAX_HOPS_STRING_SIZE 20

ProcessMessage ExperimentModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    auto &p = mp.decoded;
    LOG_INFO("ExperimentModule: from=0x%0x, to=0x%0x, channel=%u, rx_time=0x%0x, rx_snr=%f, hop_limit=%u, rx_rssi=%i, hop_start=%u, id=0x%x, msg=%.*s", mp.from, mp.to, mp.channel, mp.rx_time, mp.rx_snr, mp.hop_limit, mp.rx_rssi, mp.hop_start, mp.id, p.payload.size, p.payload.bytes);

    if (startsWith(p.payload.bytes, "ping")) {
        handlePingMessage(mp);
    } else if (startsWith(p.payload.bytes, "test")) {
        handleTestMessage(mp);
    }

    return ProcessMessage::CONTINUE;
}

bool ExperimentModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

int ExperimentModule::startsWith(const unsigned char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str++) != tolower((unsigned char)*prefix++)) {
            return 0;
        }
    }
    return 1;
}

void ExperimentModule::handlePingMessage(const meshtastic_MeshPacket &mp) {
    sendReplyMessage(mp, "pong 🏓");
}

void ExperimentModule::handleTestMessage(const meshtastic_MeshPacket &mp) {
    sendReplyMessage(mp, "check ✅");
}

void ExperimentModule::sendReplyMessage(const meshtastic_MeshPacket &mp, const char *replyMessage) {
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        LOG_ERROR("ExperimentModule: Failed to allocate meshtastic_MeshPacket!");
        return;
    }

    // per default reply to sender directly
    p->to = mp.from;
    p->decoded.want_response = false;
    // TODO: set priority?
    p->priority = meshtastic_MeshPacket_Priority_DEFAULT;
    // p->priority = mp.priority;
    // NOTE: may leave unset?
    // https://github.com/meshtastic/firmware/blob/7d8e0ede6ccb4c621c70589834562276cb128687/src/mesh/generated/meshtastic/mesh.pb.h#L706
    p->channel = mp.channel;
    p->want_ack = false;

    // prefix for message
    const static char *messagePrefix = "🤖";
    // buffer for the radio stats
    static char signalString[EXPMOD_MAX_SIGNAL_STRING_SIZE];
    size_t signalStringSize = sizeof(signalString);
    // buffer for the hops text
    static char hopsString[EXPMOD_MAX_HOPS_STRING_SIZE];
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
        LOG_ERROR("ExperimentModule: Failed to assemble signal string size: %d vs %d", signalStringSize, nchars);
        return;
    }

    // assemble hop text
    if (mp.hop_start - mp.hop_limit) {
        nchars = snprintf(
            hopsString,
            hopsStringSize,
            "via %u hops",
            (mp.hop_start-mp.hop_limit)
        );
    } else {
        // could also us strncpy
        nchars = snprintf(
            hopsString,
            hopsStringSize,
            "%s", "direct"
        );
    }
    if (nchars <= 0 || nchars > signalStringSize) {
        LOG_ERROR("ExperimentModule: Failed to assemble hops string size: %d vs %d", hopsStringSize, nchars);
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
            LOG_WARN("ExperimentModule: Node (0x%0x) not in NodeDB", mp.from);
            nchars = snprintf(
                replyString,
                replyStringSize,
                "%s @!%0x %s\n%s %s",
                messagePrefix, mp.from, replyMessage, signalString, hopsString
            );
        }
    // handle edge cases
    } else {
        LOG_WARN("ExperimentModule: Unhandled message. Ignoring");
        return;
    }

    // send the message
    if (nchars > 0 && nchars <= replyStringSize) {
        p->decoded.payload.size = strlen(replyString);
        memcpy(p->decoded.payload.bytes, replyString, p->decoded.payload.size);
        if (airTime->isTxAllowedChannelUtil(true)) {
            service->sendToMesh(p);
            LOG_INFO("ExperimentModule: reply send");
        } else {
            LOG_WARN("ExperimentModule: can not send, air time exceeded");
        }
    } else {
        LOG_ERROR("ExperimentModule: Failed to assemble reply string size: %d vs %d", replyStringSize, nchars);
    }
}
