#include "ExperimentModule.h"
#include "DebugConfiguration.h"
#include "MeshModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "main.h"
#include "meshtastic/mesh.pb.h"


ProcessMessage ExperimentModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#ifdef DEBUG_PORT
    auto &p = mp.decoded;
    LOG_INFO("ExperimentModule: from=0x%0x, to=0x%0x, channel=%u, rx_time=0x%0x, rx_snr=%f, hop_limit=%u, rx_rssi=%i, hop_start=%u, id=0x%x, msg=%.*s", mp.from, mp.to, mp.channel, mp.rx_time, mp.rx_snr, mp.hop_limit, mp.rx_rssi, mp.hop_start, mp.id, p.payload.size, p.payload.bytes);
#endif

    // only reply to private msg
    if (isToUs(&mp)) {
#ifdef DEBUG_PORT
        LOG_INFO("ExperimentModule: message for us, send reply.");
#endif
        // TODO: use allocReply implementation?
        meshtastic_MeshPacket *rp = allocDataPacket();
        if (rp) {
            rp->to = mp.from;
            rp->decoded.want_response = false;
            // TODO: set priority?
            rp->priority = meshtastic_MeshPacket_Priority_DEFAULT;
            rp->channel = mp.channel;

            // TODO: unsafe
            char msgBuffer[200];
            sprintf(msgBuffer, "Reply: snr: %.2f, rssi: %i, hops: %u", mp.rx_snr, mp.rx_rssi, (mp.hop_start-mp.hop_limit));
            rp->decoded.payload.size = strlen(msgBuffer);
            memcpy(rp->decoded.payload.bytes, msgBuffer, rp->decoded.payload.size);
            service->sendToMesh(rp);
#ifdef DEBUG_PORT
            LOG_INFO("ExperimentModule: reply send?");
#endif
        } else {
#ifdef DEBUG_PORT
        LOG_INFO("ExperimentModule: faild to allocate...");
#endif
        
        }
    }
    
    return ProcessMessage::CONTINUE;
}

bool ExperimentModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}
