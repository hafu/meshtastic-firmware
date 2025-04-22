#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "AirQualityTelemetry.h"
#include "Default.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "RTC.h"
#include "Router.h"
#include "detect/ScanI2CTwoWire.h"
#include "main.h"
#include <Throttle.h>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL
// sensors
#include "Sensor/PMSA003ISensor.h"

PMSA003ISensor pmsa003iSensor;
#endif

int32_t AirQualityTelemetryModule::runOnce()
{
    uint32_t result = UINT32_MAX;
    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */

    // moduleConfig.telemetry.air_quality_enabled = 1;

    if (!(moduleConfig.telemetry.air_quality_enabled)) {
        // If this module is not enabled, and the user doesn't want the display screen don't waste any OSThread time on it
        return disable();
    }

    if (firstTime) {
        // This is the first time the OSThread library has called this function, so do some setup
        firstTime = false;

        if (moduleConfig.telemetry.air_quality_enabled) {
            LOG_INFO("Air quality Telemetry: init");
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL
            if (pmsa003iSensor.hasSensor())
                result = pmsa003iSensor.runOnce();
#endif
        }
        return result == UINT32_MAX ? disable() : setStartDelay();
    } else {
        // if we somehow got to a second run of this module with measurement disabled, then just wait forever
        if (!moduleConfig.telemetry.air_quality_enabled)
            return disable();

        if (((lastSentToMesh == 0) ||
             !Throttle::isWithinTimespanMs(lastSentToMesh, Default::getConfiguredOrDefaultMsScaled(
                                                               moduleConfig.telemetry.air_quality_interval,
                                                               default_telemetry_broadcast_interval_secs, numOnlineNodes))) &&
            airTime->isTxAllowedChannelUtil(config.device.role != meshtastic_Config_DeviceConfig_Role_SENSOR) &&
            airTime->isTxAllowedAirUtil()) {
            sendTelemetry();
            lastSentToMesh = millis();
        } else if (service->isToPhoneQueueEmpty()) {
            // Just send to phone when it's not our time to send to mesh yet
            // Only send while queue is empty (phone assumed connected)
            sendTelemetry(NODENUM_BROADCAST, true);
            lastSentToPhone = millis();
        }
    }
    return min(sendToPhoneIntervalMs, result);
}

bool AirQualityTelemetryModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_Telemetry *t)
{
    if (t->which_variant == meshtastic_Telemetry_air_quality_metrics_tag) {
#ifdef DEBUG_PORT
        const char *sender = getSenderShortName(mp);

        LOG_INFO("(Received from %s): pm10_standard=%i, pm25_standard=%i, pm100_standard=%i", sender,
                 t->variant.air_quality_metrics.pm10_standard, t->variant.air_quality_metrics.pm25_standard,
                 t->variant.air_quality_metrics.pm100_standard);

        LOG_INFO("                  | PM1.0(Environmental)=%i, PM2.5(Environmental)=%i, PM10.0(Environmental)=%i",
                 t->variant.air_quality_metrics.pm10_environmental, t->variant.air_quality_metrics.pm25_environmental,
                 t->variant.air_quality_metrics.pm100_environmental);
#endif
        // release previous packet before occupying a new spot
        if (lastMeasurementPacket != nullptr)
            packetPool.release(lastMeasurementPacket);

        lastMeasurementPacket = packetPool.allocCopy(mp);
    }

    return false; // Let others look at this message also if they want
}

bool AirQualityTelemetryModule::getAirQualityTelemetry(meshtastic_Telemetry *m)
{
    bool valid = true;
    bool hasSensor = false;

    m->time = getTime();
    m->which_variant = meshtastic_Telemetry_air_quality_metrics_tag;
    m->variant.air_quality_metrics = meshtastic_AirQualityMetrics_init_zero;
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL
    if (pmsa003iSensor.hasSensor()) {
        valid = valid && pmsa003iSensor.getMetrics(m);
        hasSensor = true;
    }
#endif

    LOG_INFO("Send: PM1.0(Standard)=%i, PM2.5(Standard)=%i, PM10.0(Standard)=%i", m->variant.air_quality_metrics.pm10_standard,
             m->variant.air_quality_metrics.pm25_standard, m->variant.air_quality_metrics.pm100_standard);

    LOG_INFO("         | PM1.0(Environmental)=%i, PM2.5(Environmental)=%i, PM10.0(Environmental)=%i",
             m->variant.air_quality_metrics.pm10_environmental, m->variant.air_quality_metrics.pm25_environmental,
             m->variant.air_quality_metrics.pm100_environmental);

    return valid && hasSensor;
}

meshtastic_MeshPacket *AirQualityTelemetryModule::allocReply()
{
    if (currentRequest) {
        auto req = *currentRequest;
        const auto &p = req.decoded;
        meshtastic_Telemetry scratch;
        meshtastic_Telemetry *decoded = NULL;
        memset(&scratch, 0, sizeof(scratch));
        if (pb_decode_from_bytes(p.payload.bytes, p.payload.size, &meshtastic_Telemetry_msg, &scratch)) {
            decoded = &scratch;
        } else {
            LOG_ERROR("Error decoding AirQualityTelemetry module!");
            return NULL;
        }
        // Check for a request for air quality metrics
        if (decoded->which_variant == meshtastic_Telemetry_air_quality_metrics_tag) {
            meshtastic_Telemetry m = meshtastic_Telemetry_init_zero;
            if (getAirQualityTelemetry(&m)) {
                LOG_INFO("Air quality telemetry reply to request");
                return allocDataProtobuf(m);
            } else {
                return NULL;
            }
        }
    }
    return NULL;
}

bool AirQualityTelemetryModule::sendTelemetry(NodeNum dest, bool phoneOnly)
{
    meshtastic_Telemetry m = meshtastic_Telemetry_init_zero;
    if (getAirQualityTelemetry(&m)) {
        meshtastic_MeshPacket *p = allocDataProtobuf(m);
        p->to = dest;
        p->decoded.want_response = false;
        if (config.device.role == meshtastic_Config_DeviceConfig_Role_SENSOR)
            p->priority = meshtastic_MeshPacket_Priority_RELIABLE;
        else
            p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;

        // release previous packet before occupying a new spot
        if (lastMeasurementPacket != nullptr)
            packetPool.release(lastMeasurementPacket);

        lastMeasurementPacket = packetPool.allocCopy(*p);
        if (phoneOnly) {
            LOG_INFO("Send packet to phone");
            service->sendToPhone(p);
        } else {
            LOG_INFO("Send packet to mesh");
            service->sendToMesh(p, RX_SRC_LOCAL, true);
        }
        return true;
    }

    return false;
}

#endif
