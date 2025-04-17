#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"
#include <Adafruit_SCD30.h>

class SCD30Sensor : public TelemetrySensor
{
  private:
    Adafruit_SCD30 scd30;
    uint16_t measurement_interval = 2; // default value in seconds

  protected:
    virtual void setup() override;

  public:
    SCD30Sensor();
    virtual int32_t runOnce() override;
    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
};

#endif
