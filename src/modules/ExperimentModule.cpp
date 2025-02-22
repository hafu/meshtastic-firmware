#include "ExperimentModule.h"
#include "DebugConfiguration.h"
#include "MeshModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RadioInterface.h"
#include "configuration.h"
#include "main.h"
#include "meshtastic/mesh.pb.h"
#include <cstddef>


ProcessMessage ExperimentModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    auto &p = mp.decoded;
    LOG_INFO("ExperimentModule: from=0x%0x, to=0x%0x, channel=%u, rx_time=0x%0x, rx_snr=%f, hop_limit=%u, rx_rssi=%i, hop_start=%u, id=0x%x, msg=%.*s", mp.from, mp.to, mp.channel, mp.rx_time, mp.rx_snr, mp.hop_limit, mp.rx_rssi, mp.hop_start, mp.id, p.payload.size, p.payload.bytes);

    // only reply to private msg
    if (isToUs(&mp) && startsWith(p.payload.bytes, "ping")) {
        LOG_INFO("ExperimentModule: message for us, send reply.");
        // TODO: use allocReply implementation?
        meshtastic_MeshPacket *rp = allocDataPacket();
        if (rp) {
            rp->to = mp.from;
            rp->decoded.want_response = false;
            // TODO: set priority?
            // rp->priority = meshtastic_MeshPacket_Priority_DEFAULT;
            rp->priority = mp.priority;
            rp->channel = mp.channel;
            rp->want_ack = false;

            static char replyString[MAX_LORA_PAYLOAD_LEN+1];
            size_t replyStringSize = sizeof(replyString);
            int nchars;

            nchars = snprintf(replyString, replyStringSize, "🤖 pong 🏓 snr: %.2f, rssi: %i, hops: %u", mp.rx_snr, mp.rx_rssi, (mp.hop_start-mp.hop_limit));
            if (nchars > 0 && nchars <= replyStringSize) {
                rp->decoded.payload.size = strlen(replyString);
                memcpy(rp->decoded.payload.bytes, replyString, rp->decoded.payload.size);
                service->sendToMesh(rp);
                LOG_INFO("ExperimentModule: reply send?");
            } else {
                LOG_INFO("ExperimentModule: Buffer overflow? %d available: %d", nchars, replyStringSize);
            }
        } else {
            LOG_INFO("ExperimentModule: faild to allocate...");
        }
    }
    
    return ProcessMessage::CONTINUE;
}

bool ExperimentModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

int ExperimentModule::startsWith(const unsigned char *str, const char *prefix) {
    while (*prefix && std::tolower((unsigned char)*prefix) == std::tolower((unsigned char)*str)) {
        prefix++;
        str++;
    }
    return *prefix == '\0';
}
