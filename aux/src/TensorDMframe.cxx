#include "WireCellAux/TensorDMframe.h"
#include "WireCellAux/TensorDMdataset.h"
#include "WireCellAux/FrameTools.h"
#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleTrace.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellUtil/Type.h"
// #include "WireCellUtil/Logging.h" // debug
#include "WireCellUtil/PointCloudDataset.h"
#include <boost/filesystem.hpp>

#include <sstream>

using namespace WireCell;
namespace PC = WireCell::PointCloud;
using namespace WireCell::Aux;
using namespace WireCell::Aux::TensorDM;
using namespace boost::filesystem;

// template<typename T>
// void dump_array(const PC::Dataset& ds, const std::string& name, const std::string& prefix = "\t\t")
// {
//     if (! ds.has(name)) {
//         spdlog::debug("tensordm frame: {} no array \"{}\"", prefix, name);
//         return;
//     }

//     auto arr = ds.get(name);
//     assert(arr->is_type<T>());

//     std::stringstream ss;
//     ss << "doctest_tensordm_frame: array: " << prefix << "\n";
//     ss << "\tarray \"" << name << "\" size major: " << arr->size_major() << " dtype: " << arr->dtype() << "\n";
//     ss << "\tmetadata: " << arr->metadata() << "\n";
//     auto vec = arr->elements<T>();
//     ss << vec.size() << " " << name;
//     for (auto one : vec) ss << " " << one;
//     spdlog::debug(ss.str());

// }

// static void dump_dataset(const PC::Dataset& ds, const std::string& ctx)
// {
//     std::stringstream ss;
//     ss << "dataset: " << ctx << " " << ds.size() << "x" << ds.size_major() << "\n";
//     ss << "\tmetadata: " << ds.metadata() << "\n";
//     for (const auto& key : ds.keys()) {
//         auto arr = ds.get(key);
//         ss <<"\t" << key << ": (" << arr->dtype() << ") [" << arr->size_major()<<"] " << arr->bytes().size() << " bytes, metadata: " << arr->metadata() << "\n";
//     }
//     spdlog::debug(ss.str());
// }


Configuration
WireCell::Aux::TensorDM::as_config(const Waveform::ChannelMasks& cms)
{
    Configuration ret = Json::objectValue;
    for (const auto& [chid, br] : cms) {
        Configuration each = Json::arrayValue;
        for (const auto& [first,second] : br) {
            Configuration pair = Json::arrayValue;
            pair.append(first);
            pair.append(second);
            each.append(pair);
        }
        // The integer valued chids must be stringified to serve as
        // object keys else JsonCPP tries to interpret ret as an
        // array.
        const std::string chid_str = std::to_string(chid);
        ret[chid_str] = each;
    }
    return ret;
}

Configuration
WireCell::Aux::TensorDM::as_config(const Waveform::ChannelMaskMap& cmm)
{
    Configuration ret = Json::objectValue;
    for (const auto& [tag, cms] : cmm) {
        ret[tag] = as_config(cms);
    }
    return ret;
}

ITensor::pointer
WireCell::Aux::TensorDM::as_trace_tensor(const ITrace::vector& traces,
                                         const std::string& dpath,
                                         bool truncate,
                                         std::function<float(float)> transform)
{
    auto chids = Aux::channels(traces);
    auto tmm = Aux::tbin_range(traces);
    const size_t nticks = tmm.second-tmm.first;
    const ITensor::shape_t shape = {chids.size(), nticks};
    Array::array_xxf array(shape[0], shape[1]);
    Aux::fill(array, traces, chids.begin(), chids.end(), tmm.first);
    Array::array_xxf trans = array.unaryExpr(transform);
    Configuration tmd;
    tmd["datatype"] = "trace";
    tmd["datapath"] = dpath;
    tmd["tbin"] = tmm.first;

    if (truncate) {
        using ROWI = Eigen::Array<int16_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        ROWI itrans = trans.cast<int16_t>();
        return std::make_shared<SimpleTensor>(shape, itrans.data(), tmd);
    }
    using ROWF = Eigen::Array<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    ROWF ftrans = trans;
    return std::make_shared<SimpleTensor>(shape, ftrans.data(), tmd);
}

ITensor::pointer
WireCell::Aux::TensorDM::as_trace_tensor(ITrace::pointer trace,
                                         const std::string& dpath, 
                                         bool truncate,
                                         std::function<float(float)> transform)                              
{
    Configuration tmd;
    tmd["datatype"] = "trace";
    tmd["datapath"] = dpath;
    tmd["tbin"] = trace->tbin();

    auto q = trace->charge();
    std::transform(q.cbegin(), q.cend(), q.begin(), transform);
    const ITensor::shape_t shape = {q.size()};
    if (truncate) {
        std::vector<int16_t> iq(q.size());
        std::transform(q.cbegin(), q.cend(), iq.begin(), [](float x) -> int16_t { return (int16_t)x; });
        return std::make_shared<SimpleTensor>(shape, iq.data(), tmd);
    }
    return std::make_shared<SimpleTensor>(shape, q.data(), tmd);
}


ITensor::vector
WireCell::Aux::TensorDM::as_tensors(IFrame::pointer frame,
                                    const std::string& datapath,
                                    FrameTensorMode mode,
                                    bool truncate,
                                    std::function<float(float)> transform)
{
    Configuration fmd;
    fmd["datatype"] = "frame";
    fmd["datapath"] = datapath;
    fmd["ident"] = frame->ident();
    fmd["time"] = frame->time();
    fmd["tick"] = frame->tick();
    assign(fmd["tags"], frame->frame_tags());
    fmd["masks"] = as_config(frame->masks());
    fmd["traces"] = Json::arrayValue;
    fmd["tracedata"] = Json::arrayValue;

    boost::filesystem::path tr_dpath = datapath + "/traces";
    boost::filesystem::path td_dpath = datapath + "/tracedata";

    ITensor::vector tr_tens, td_tens;

    // Things go under datapath/{traces,tracedata}/{tag}
    if (mode == FrameTensorMode::tagged) {
        size_t ntraces = 0;
        for (const auto& tag : frame->trace_tags()) {

            auto tts = tagged_traces(frame, tag);
            auto trace_path = (tr_dpath / tag).string();
            auto iten = as_trace_tensor(tts, trace_path, truncate, transform);
            tr_tens.push_back(iten);
            fmd["traces"].append(trace_path);

            PC::Dataset ds;
            ds.metadata()["tag"] = tag;
            ds.add("chid", PC::Array(Aux::channels(tts)));
            // assert(ds.get("chid")->elements<int>().size() == tts.size());
            std::vector<size_t> indices(tts.size());
            std::iota(indices.begin(), indices.end(), ntraces);
            ds.add("index", PC::Array(indices));
            auto summary = frame->trace_summary(tag);
            if (summary.size() == tts.size()) {
                ds.add("summary", PC::Array(summary));
            }
            auto tracedata_path = (td_dpath / tag).string();
            auto tds = as_tensors(ds, tracedata_path);
            td_tens.insert(td_tens.end(), tds.begin(), tds.end());
            fmd["tracedata"].append(tracedata_path);

            ntraces += tts.size();
        }
    }

    else {                      // sparse or unified
        const ITrace::vector& traces = *(frame->traces().get());

        // Traces datapath/traces
        if (mode == FrameTensorMode::unified) {
            auto trace_path = tr_dpath.string();
            auto iten = as_trace_tensor(traces, trace_path, truncate, transform);
            tr_tens.push_back(iten);
            fmd["traces"].append(trace_path);
        }

        // Traces datapath/traces/NNN
        else {                  // sparse
            int count=0;
            for (const auto& trace : traces) {
                auto trace_path = (tr_dpath / std::to_string(count++)).string();
                auto iten = as_trace_tensor(trace, trace_path, truncate, transform);
                tr_tens.push_back(iten);
                fmd["traces"].append(trace_path);
            }
        }

        // Both unified and sparse have same handling the rest.

        {   // datapath/tracedata/_ for single chids spanning all
            PC::Dataset ds;         // no tag
            ds.add("chid", PC::Array(Aux::channels(traces)));
            // assert(ds.get("chid")->elements<int>().size() == traces.size());
            auto tracedata_path = (td_dpath / "_").string();
            auto tds = as_tensors(ds, tracedata_path);
            td_tens.insert(td_tens.end(), tds.begin(), tds.end());
            fmd["tracedata"].append(tracedata_path);
        }
        
        // datapath/tracedata/tag for tagged traces with optional summaries
        for (const auto& tag : frame->trace_tags()) {
            PC::Dataset ds;
            ds.metadata()["tag"] = tag;
            const std::vector<size_t>& index = frame->tagged_traces(tag);
            ds.add("index", PC::Array(index));
            auto summary = frame->trace_summary(tag);
            if (summary.size() > 0) {
                ds.add("summary", PC::Array(summary));
            }
            auto tracedata_path = (td_dpath / tag).string();
            auto tds = as_tensors(ds, tracedata_path);
            td_tens.insert(td_tens.end(), tds.begin(), tds.end());
            fmd["tracedata"].append(tracedata_path);
        }        
    }

    ITensor::vector ret;
    ret.push_back(std::make_shared<SimpleTensor>(fmd));
    ret.insert(ret.end(), tr_tens.begin(), tr_tens.end());
    ret.insert(ret.end(), td_tens.begin(), td_tens.end());
    return ret;
}

Waveform::ChannelMaskMap WireCell::Aux::TensorDM::as_cmm(const Configuration& jcmm)
{
    Waveform::ChannelMaskMap cmm;
    if (jcmm.isNull()) {
        return cmm;
    }

    for (auto jcmm_it = jcmm.begin(); jcmm_it != jcmm.end(); ++jcmm_it) {

        const auto& jtag = jcmm_it.key();
        const auto& jcms = *jcmm_it;
        auto& cms = cmm[jtag.asString()];

        for (auto jcms_it = jcms.begin(); jcms_it != jcms.end(); ++jcms_it) {

            const auto& jchid = jcms_it.key();
            const auto& jbrl = *jcms_it;

            // We need string type keys but they really hold ints.
            const int ichid = atoi(jchid.asString().c_str());
            auto& brl = cms[ichid];

            for (const auto& jpair : jbrl) {
                brl.push_back(std::make_pair(jpair[0].asInt(),
                                             jpair[1].asInt()));
            }
        }
    }
    return cmm;
}

// Form trace from trace tensor.  This is not exposed in the API as we
// want to return a non-const, concrete SimpleTrace for later mutating.
static std::shared_ptr<SimpleTrace>
reintraceenate(ITensor::pointer ten, int tbin, std::function<float(float)> transform, size_t row=0)
{
    const auto shape = ten->shape();
    const size_t ndims = shape.size();
    const size_t ncols = shape[ndims-1];

    std::vector<float> q;
    if (dtype(ten->element_type()) == dtype<int16_t>()) {
        const int16_t* beg = (const int16_t*)ten->data() + row*ncols;
        q.assign(beg, beg+ncols);
    }
    else if (dtype(ten->element_type()) == dtype<float>()) {
        const float* beg = (const float*)ten->data() + row*ncols;
        q.assign(beg, beg+ncols);
    }
    else {
        THROW(ValueError() << errmsg{"bad numeric type for trace"});
    }
    std::transform(q.cbegin(), q.cend(), q.begin(), transform);

    const int def_chid=-1;
    return std::make_shared<SimpleTrace>(def_chid, tbin, q);
}

IFrame::pointer WireCell::Aux::TensorDM::as_frame(const ITensor::vector& tens,
                                                  const std::string& datapath,
                                                  std::function<float(float)> transform)
{
    const auto& located = index_datapaths(tens);
    ITensor::pointer ften=top_tensor(tens, "frame", datapath, located);

    if (!ften) {
        THROW(ValueError() << errmsg{"no frame tensor found"});
    }

    auto fmd = ften->metadata();
    // traces, sans chids
    std::vector<std::shared_ptr<SimpleTrace>> straces;
    for (auto jtrpath : fmd["traces"]) {
        auto trpath = jtrpath.asString();
        auto trten = located.at(trpath);
        auto trmd = trten->metadata();

        int tbin = get(trmd, "tbin", 0);
        const auto shape = trten->shape();
        const size_t ndims = shape.size();
        if (ndims < 1 or ndims > 2) {
            THROW(ValueError() << errmsg{"bad trace array shape"});
        }
        if (shape.size() == 1) { // single sparse trace
            straces.push_back(reintraceenate(trten, tbin, transform));
            continue;
        }
        const size_t nrows = shape[0];
        for (size_t row = 0; row < nrows; ++row) {
            straces.push_back(reintraceenate(trten, tbin, transform, row));
        }
    }

    ITrace::vector itraces(straces.begin(), straces.end());
    auto sf = std::make_shared<SimpleFrame>(fmd["ident"].asInt(),
                                            fmd["time"].asDouble(),
                                            itraces,
                                            fmd["tick"].asDouble(),
                                            as_cmm(fmd["cmm"]));
    // trace data
    if (fmd["tracedata"].isArray()) {
        for (auto jtdpath : fmd["tracedata"]) {
            if (jtdpath.isNull() or ! jtdpath.isString()) {
                raise<ValueError>("malformed tracedata datapath in frame tensor");
            }
            auto tdpath = jtdpath.asString();
            const bool share = false; // was true
            PC::Dataset td = as_dataset(tens, tdpath, located, share);
            // dump_dataset(td, tdpath);
            auto tdmd = td.metadata();
    
            auto tag = get<std::string>(tdmd, "tag", "");
            if (tag.empty()) {      // whole
                auto achid = td.get("chid");
                // assert(achid);
                // assert(achid->size_major() == straces.size());
                // assert(achid->is_type<int>());
                auto chid = achid->elements<int>();
                const size_t num = chid.size();
                // assert(num == straces.size());
                if (num != straces.size()) {
                    raise<ValueError>("tagless tracedata chid array size=%d mismatch with traces=%d",
                                      num, straces.size());
                }
                for (size_t ind=0; ind<num; ++ind) {
                    straces[ind]->m_chid = chid[ind];
                }
                continue;
            }

            // tagged traces, perhaps with summaries
            if (! td.has("index")) {
                raise<ValueError>("tagged tracedata has no index for tag %s", tag);
            }
            auto index = td.get("index")->elements<size_t>();
            if (td.has("summary")) {
                auto summary = td.get("summary")->elements<double>();
                sf->tag_traces(tag, index, summary);
            }
            else {
                sf->tag_traces(tag, index);
            }
            if (td.has("chid")) {
                auto chid = td.get("chid")->elements<int>();
                const size_t num = chid.size();
                if (num != index.size()) {
                    THROW(ValueError() << errmsg{"tagged tracedata chid array size mismatch"});
                }
                for (size_t ind=0; ind<num; ++ind) {
                    straces[index[ind]]->m_chid = chid[ind];
                }                
            }
        }        
    }
    
    if (fmd["tags"].isArray()) {
        for (auto jtag : fmd["tags"]) {
            sf->tag_frame(jtag.asString());
        }
    }

    return sf;
}


