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

SCD30Sensor *SCD30Sensor::getInstance()
{
    if (pinstance == nullptr) {
        LOG_DEBUG("SCD30Sensor: new instance");
        pinstance = new SCD30Sensor();
    }
    return pinstance;
}

SCD30Sensor *SCD30Sensor::pinstance{nullptr};

int32_t SCD30Sensor::runOnce()
{
    LOG_INFO("Init sensor: %s", sensorName);
    if (!hasSensor()) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }

    if (first_run) {
        first_run = false;

        scd30 = Adafruit_SCD30();
        status = scd30.begin(nodeTelemetrySensorsMap[sensorType].first, nodeTelemetrySensorsMap[sensorType].second);
        if (!status) {
            LOG_ERROR("SCD30Sensor: begin error: %i", status);
        }
        LOG_DEBUG("SCD30Sensor: begin cmd OK");

        // NOTE: may use value from configuration?
        // status = scd30.setAltitudeOffset(0);
        // if (!status) {
        //   LOG_ERROR("SCD30Sensor: setAltitudeOffset: %i", status);
        // }
        // LOG_DEBUG("SCD30Sensor: setAltitudeOffset cmd OK");
        // LOG_DEBUG("SCD30Sensor: setAltitudeOffset is %i", scd30.getAltitudeOffset());

        // make sure self calibration is disabled
        status = scd30.selfCalibrationEnabled(false);
        if (!status) {
            LOG_ERROR("SCD30Sensor: selfCalibrationEnabled: %i", status);
        }
        LOG_DEBUG("SCD30Sensor: selfCalibrationEnabled cmd OK");
        LOG_DEBUG("SCD30Sensor: selfCalibrationEnabled is %i", scd30.selfCalibrationEnabled());

        // set measurement interval
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
            LOG_ERROR("SCD30Sensor: setMeasurementIntervalerror: %i", status);
        }
        LOG_DEBUG("SCD30Sensor: setMeasurementIntervalerror cmd OK");
        LOG_DEBUG("SCD30Sensor: getMeasurementInterval: %i", scd30.getMeasurementInterval());

        status = scd30.startContinuousMeasurement();
        if (!status) {
            LOG_ERROR("SCD30Sensor: startContinuousMeasurement: %i", status);
        }
        LOG_DEBUG("SCD30Sensor: startContinuousMeasurement cmd OK");
    } else {
        LOG_DEBUG("SCD30Sensor: already intilized.");
    }

    return initI2CSensor();
}

void SCD30Sensor::setup() {}

bool SCD30Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    /*
     * - only read data from register if available/ready and time
     *   of the sensors measurement interval exceeded
     * - if we try to read data faster than the measurement interval
     *   the sensor is not ready and if we use read() ignoring
     *   dataReady it will give you rubbish data
     * - this ensures that the latest available measurement is
     *   reported or the last (cached) read value
     */
    if (last_read_ms == 0 || (millis() - last_read_ms) >= measurement_interval * 1000) {
        if (!scd30.dataReady()) {
            LOG_WARN("SCD30Sensor: Data not ready");
            return false;
        }
        LOG_DEBUG("SCD30Sensor: Try to read data");
        if (!scd30.read()) {
            LOG_WARN("SCD30Sensor: Could not read data");
            //   return false;
        } else {
            last_read_ms = millis();
            LOG_DEBUG("SCD30Sensor: Last read set to %i", last_read_ms);
        }
    } else {
        LOG_DEBUG("SCD30Sensor: Using cached values");
    }

    // if CO2 value is higher than lowest value we have a valid measurement
    if (scd30.CO2 < 400.0) {
        LOG_WARN("SCD30Sensor: Skip send measuremnts, no valid data available.");
        return false;
    }

    LOG_DEBUG("SCD30Sensor: co2: %.2f, temperature: %.2f, humidity: %.2f", scd30.CO2, scd30.temperature, scd30.relative_humidity);

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
