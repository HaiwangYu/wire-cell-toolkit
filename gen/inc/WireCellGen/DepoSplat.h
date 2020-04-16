/** This class "splats" depos directly into a frame without regards to
 * much reality.  It's only useful for gross, but fast debugging jobs.
 * The frame it produces is the moral equivalent of post-SP.
 */

#ifndef WIRECELLGEN_DEPOSPLAT
#define WIRECELLGEN_DEPOSPLAT

#include "WireCellGen/Ductor.h"

namespace WireCell
{
    namespace Gen
    {
        // DepoSplat inherits from Ductor, replacing the heavy lifting
        // with some lightweight laziness.
        class DepoSplat : public Ductor
        {
           public:
            DepoSplat();
            virtual ~DepoSplat();

           protected:
            virtual ITrace::vector process_face(IAnodeFace::pointer face,
                                                const IDepo::vector &depos);

            /// SPD logger
            Log::logptr_t l;
        };
    }  // namespace Gen
}  // namespace WireCell

#endif
