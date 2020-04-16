/** An ElecResponse waveform is that due to sending a unit charge
 * pulse into an amplifier with gain and shaping and a particular
 * response function of the BNL cold electronics.  */

#ifndef WIRECELL_GEN_ELECRESPONSE
#define WIRECELL_GEN_ELECRESPONSE

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IWaveform.h"
#include "WireCellUtil/Units.h"

namespace WireCell
{
    namespace Gen
    {
        class ElecResponse : public IWaveform, public IConfigurable
        {
           public:
            ElecResponse(int nticks = 10000, double t0 = 0,
                         double gain = 14.0 * units::mV / units::fC,
                         double shaping = 2 * units::us, double postgain = 1.0,
                         double tick = 0.5 * units::us);
            // IConfigurable interface
            virtual void configure(const WireCell::Configuration &cfg);
            virtual WireCell::Configuration default_configuration() const;

            // The starting point of the sampling
            virtual double waveform_start() const;
            // The sampling period aka bin width
            virtual double waveform_period() const;
            // The collection of samples
            virtual const sequence_type &waveform_samples() const;

           private:
            Configuration m_cfg;
            sequence_type m_wave;
        };
    }  // namespace Gen
}  // namespace WireCell

#endif
