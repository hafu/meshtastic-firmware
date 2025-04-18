#include "Adafruit_BME280.h"
#include "DebugConfiguration.h"
#include "configuration.h"
#include "delay.h"
#include <cstdint>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "SCD30Sensor.h"
#include "TelemetrySensor.h"
#include <Adafruit_SCD30.h>

SCD30Sensor::SCD30Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_SCD30, "SCD30") {}

int32_t SCD30Sensor::runOnce()
{
    LOG_INFO("Init sensor: %s", sensorName);
    if (!hasSensor()) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }

    scd30 = Adafruit_SCD30();
    status = scd30.begin(nodeTelemetrySensorsMap[sensorType].first, nodeTelemetrySensorsMap[sensorType].second);
    if (!status) {
        LOG_ERROR("SCD30: begin error: %i", status);
    }
    LOG_DEBUG("SCD30 begin cmd OK");

    scd30.reset();
    delay(2000);

    // status = scd30.setAltitudeOffset(50);
    // if (!status) {
    //   LOG_ERROR("SCD30: setAltitudeOffset: %i", status);
    // }
    // LOG_DEBUG("SCD30 setAltitudeOffset cmd OK");
    // LOG_DEBUG("SCD30 setAltitudeOffset is %i", scd30.getAltitudeOffset());

    status = scd30.selfCalibrationEnabled(false);
    if (!status) {
        LOG_ERROR("SCD30: selfCalibrationEnabled: %i", status);
    }
    LOG_DEBUG("SCD30 selfCalibrationEnabled cmd OK");
    LOG_DEBUG("SCD30 selfCalibrationEnabled is %i", scd30.selfCalibrationEnabled());

    // set measurement interval twice the configured interval
    // NOTE: or set it to the configured value?
    // https://sensirion.com/media/documents/0FEA2450/61652EF9/Sensirion_CO2_Sensors_SCD30_Low_Power_Mode.pdf
    // Should be between 5 - 60s
    if (moduleConfig.telemetry.air_quality_interval > 60) {
        // max interval
        measurement_interval = 60;
    } else if (moduleConfig.telemetry.air_quality_interval < 2) {
        measurement_interval = 2;
    } else {
        measurement_interval = (uint16_t)moduleConfig.telemetry.air_quality_interval;
    }
    status = scd30.setMeasurementInterval(measurement_interval);
    if (!status) {
        LOG_ERROR("SCD30: setMeasurementIntervalerror: %i", status);
    }
    LOG_DEBUG("SCD30 setMeasurementIntervalerror cmd OK");
    LOG_DEBUG("SCD30 getMeasurementInterval: %i", scd30.getMeasurementInterval());

    status = scd30.startContinuousMeasurement();
    if (!status) {
        LOG_ERROR("SCD: startContinuousMeasurement: %i", status);
    }
    LOG_DEBUG("SCD30 startContinuousMeasurement cmd OK");

    return initI2CSensor();
}

void SCD30Sensor::setup() {}

bool SCD30Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    LOG_DEBUG("SCD30Sensor::getMetrics(...)");
    // if (!scd30.dataReady()) {
    //   LOG_WARN("SCD30: Skip send measurements. Data not ready");
    //   // return false;
    // }
    if (!scd30.read()) {
        LOG_WARN("SCD30: Could not read data");
        //   return false;
    }

    // if CO2 value is higher than lowest value we have a valid measurement
    if (scd30.CO2 < 400.0) {
        LOG_WARN("SCD30: Skip send measuremnts, no valid data available.");
        return false;
    }

    LOG_DEBUG("SCD30: co2: %.2f, temperature: %.2f, humidity: %.2f", scd30.CO2, scd30.temperature, scd30.relative_humidity);

    return true;
}

bool SCD30Sensor::getAirQualityMetrics(meshtastic_Telemetry *measurement)
{
    if (getMetrics(measurement)) {
        measurement->variant.air_quality_metrics.has_co2 = true;
        measurement->variant.air_quality_metrics.co2 = scd30.CO2;
        return true;
    }
    return false;
}

bool SCD30Sensor::getEnvironmentalRelativeHumidity(meshtastic_Telemetry *measurement)
{
    if (getMetrics(measurement)) {
        measurement->variant.environment_metrics.has_relative_humidity = true;
        measurement->variant.environment_metrics.relative_humidity = scd30.relative_humidity;
        return true;
    }
    return false;
}

bool SCD30Sensor::getEnvironmentalTemperature(meshtastic_Telemetry *measurement)
{
    if (getMetrics(measurement)) {
        measurement->variant.environment_metrics.has_temperature = true;
        measurement->variant.environment_metrics.temperature = scd30.temperature;
        return true;
    }
    return false;
}

bool SCD30Sensor::getEnvironmentalMetrics(meshtastic_Telemetry *measurement)
{
    return getEnvironmentalRelativeHumidity(measurement) && getEnvironmentalTemperature(measurement);
}
#endif
