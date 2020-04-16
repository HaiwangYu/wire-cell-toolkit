#ifndef WIRECELLGEN_UTIL
#define WIRECELLGEN_UTIL

#include <vector>
#include "WireCellIface/IFrame.h"

namespace WireCell
{
    namespace Gen
    {
        /// Sum a vector of frames, returning a new one with the given
        /// ident.  The start time of the new one will be the minimum
        /// time of all frames.  The sample period (tick) of all
        /// frames must be identical.  Traces on a common channel are
        /// summed producing a single trace which covers a time domain
        /// spanning the minimum and maximum tbin of all the traces in
        /// the channel.  Zeros are padded for any intervening samples
        /// outside of any individual trace.
        IFrame::pointer sum(std::vector<IFrame::pointer> frames, int ident);

    }  // namespace Gen
}  // namespace WireCell
#endif
