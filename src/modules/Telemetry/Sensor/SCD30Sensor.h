#include "configuration.h"
#include <cstdint>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"
#include <Adafruit_SCD30.h>

class SCD30Sensor : public TelemetrySensor
{
  private:
    static SCD30Sensor *pinstance;
    SCD30Sensor();
    Adafruit_SCD30 scd30;
    uint16_t measurement_interval = 2; // default value in seconds
    bool first_run = true;
    uint32_t lastRead = 0;

  protected:
    virtual void setup() override;

  public:
    static SCD30Sensor *getInstance();
    // Delete copy constructor and assignment operator to prevent copying
    SCD30Sensor(const SCD30Sensor &) = delete;
    SCD30Sensor &operator=(const SCD30Sensor &) = delete;

    virtual int32_t runOnce() override;
    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
    bool getAirQualityMetrics(meshtastic_Telemetry *measurement);
    bool getEnvironmentalMetrics(meshtastic_Telemetry *measurement);
    bool getEnvironmentalTemperature(meshtastic_Telemetry *measurement);
    bool getEnvironmentalRelativeHumidity(meshtastic_Telemetry *measurement);
};

#endif
