#include "configuration.h"
#include <cstdint>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "PMSA003ISensor.h"
#include "TelemetrySensor.h"
#include <Adafruit_PM25AQI.h>

PMSA003ISensor::PMSA003ISensor() : TelemetrySensor(meshtastic_TelemetrySensorType_PMSA003I, "PMSA003I") {}

int32_t PMSA003ISensor::runOnce()
{
    LOG_INFO("Init sensor: %s", sensorName);
    if (!hasSensor()) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }
    aqi = Adafruit_PM25AQI();
    // Wait three seconds for sensor to boot up!
    delay(3000);
    // aqi.begin_I2C(nodeTelemetrySensorsMap[sensorType].first);
    aqi.begin_I2C();

    return initI2CSensor();
}

void PMSA003ISensor::setup() {}

bool PMSA003ISensor::getMetrics(meshtastic_Telemetry *measurement)
{
    if (!aqi.read(&data)) {
        LOG_WARN("Skip send measurements. Could not read AQIn");
        return false;
    }

    measurement->variant.air_quality_metrics.has_pm10_standard = true;
    measurement->variant.air_quality_metrics.pm10_standard = data.pm10_standard;
    measurement->variant.air_quality_metrics.has_pm25_standard = true;
    measurement->variant.air_quality_metrics.pm25_standard = data.pm25_standard;
    measurement->variant.air_quality_metrics.has_pm100_standard = true;
    measurement->variant.air_quality_metrics.pm100_standard = data.pm100_standard;

    measurement->variant.air_quality_metrics.has_pm10_environmental = true;
    measurement->variant.air_quality_metrics.pm10_environmental = data.pm10_env;
    measurement->variant.air_quality_metrics.has_pm25_environmental = true;
    measurement->variant.air_quality_metrics.pm25_environmental = data.pm25_env;
    measurement->variant.air_quality_metrics.has_pm100_environmental = true;
    measurement->variant.air_quality_metrics.pm100_environmental = data.pm100_env;

    return true;
}
#endif
