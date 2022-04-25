#ifndef WIRECELL_SIMPLEMEASURE
#define WIRECELL_SIMPLEMEASURE

#include "WireCellIface/IMeasure.h"

namespace WireCell {

    class SimpleMeasure : public IMeasure {
      public:
        SimpleMeasure(int ident, const value_t& signal={0,0}, const IChannel::vector& chans = {})
            : id(ident), sig(signal), chans(chans)
        {}
        virtual ~SimpleMeasure() {}

        // While still a concrete type, user may directly mess with
        // these values.
        int id{0};
        value_t sig{0,0};
        IChannel::vector chans{};

        // IMeasure interface methods:
        virtual value_t signal() const { return sig; }
        virtual IChannel::vector channels() const { return chans; }

        // If id is already reasonable, use it, o.w. use smallest
        // channel ident.
        virtual int ident() const {
            const int toobig = 0x7fffffff;
            if (id >= 0 and id < toobig) { return id; }
            int ret = toobig;
            for (const auto& one : chans) {
                int chid = one->ident();
                if (chid < id) {
                    ret = chid;
                }
            }
            return ret;
        }
    };

}

#endif
