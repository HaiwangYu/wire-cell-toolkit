/*! test TaggedFrameTensorSet
 *
 * Also provides an example of how to "hook" an Eigen array to 2D
 * tensor data provided by an ITensor.
 */

#include "WireCellAux/TaggedFrameTensorSet.h"
#include "WireCellAux/TaggedTensorSetFrame.h"
#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"
#include "WireCellUtil/Testing.h"

using namespace std;
using namespace WireCell;

int main()
{
    const double tick = 0.5 * units::us;
    // "Signal"
    const int signal_frame_ident = 100;
    const double signal_start_time = 6 * units::ms;
    vector<float> signal{0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 0.0};

    ITrace::vector signal_traces{make_shared<SimpleTrace>(1, 10, signal),
                                 make_shared<SimpleTrace>(1, 20, signal),
                                 make_shared<SimpleTrace>(2, 30, signal),
                                 make_shared<SimpleTrace>(2, 33, signal)};
    auto iframe1 = make_shared<SimpleFrame>(signal_frame_ident, signal_start_time,
                                            signal_traces, tick);

    ITensorSet::pointer itensorset = nullptr;
    Aux::TaggedFrameTensorSet tfts;
    auto cfg = tfts.default_configuration();
    cfg["tensors"][0]["tag"] = "";
    tfts.configure(cfg);
    bool ok = tfts(iframe1, itensorset);
    Assert(ok);
    Assert(itensorset);

    Assert(itensorset->ident() == signal_frame_ident);
    auto tens = itensorset->tensors();
    Assert(tens);
    Assert(tens->size() == 1);
    auto ten = tens->at(0);
    Assert(ten);
    auto shape = ten->shape();
    Assert(shape.size() == 2);
    cout << shape[0] << " " << shape[1] << endl;
    Assert(shape[0] == 2);
    const int nchans = shape[0];
    const int nticks = shape[1];

    auto md = itensorset->metadata();
    cout << "metadata: " << md << endl;
    Assert(get(md, "time", 0.0) == signal_start_time);
    Assert(get(md, "tick", 0.0) == tick);
    auto jtens = md["tensors"];
    Assert(jtens.size() == 1);
    auto jten = jtens[0];
    int tbin = get(jten, "tbin", -1);
    Assert(tbin == 10);
    auto jch = jten["channels"];
    Assert(nchans == (int) jch.size());  // JsonCPP type unsafety is annoying!
    Assert(jch[0].asInt() ==
           1);  // arrays must be int, not size_t, but size is size_t!
    Assert(jch[1].asInt() == 2);

    Eigen::Map<Eigen::ArrayXXf> arr((float *) ten->data(), nchans, nticks);
    std::cout << "channels:\ntick\t";
    for (int ichan = 0; ichan < nchans; ++ichan)
    {
        std::cout << jch[ichan].asInt() << "\t";
    }
    std::cout << endl;

    for (int itick = 0; itick < nticks; ++itick)
    {
        std::cout << tbin + itick;
        for (int ichan = 0; ichan < nchans; ++ichan)
        {
            std::cout << "\t" << arr(ichan, itick);
        }
        std::cout << endl;
    }

    Aux::TaggedTensorSetFrame ttsf;
    cfg = ttsf.default_configuration();
    cfg["tensors"][0]["tag"] = "";
    ttsf.configure(cfg);

    IFrame::pointer iframe2;
    ok = ttsf(itensorset, iframe2);
    Assert(ok);
    Assert(iframe2);
    {
        auto tt = iframe2->trace_tags();
        Assert(tt.size() == 1);
        Assert(tt[0] == "");
    }
    {
        auto traces = iframe2->traces();
        Assert(traces->size() == 2);
        Assert(traces->at(0)->channel() == 1);
        Assert(traces->at(1)->channel() == 2);
    }
    {
        auto tt = iframe2->tagged_traces("");
        Assert(tt.size() == 2);
    }

    return 0;
}
