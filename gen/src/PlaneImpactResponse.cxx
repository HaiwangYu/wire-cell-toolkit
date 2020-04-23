#include "WireCellGen/PlaneImpactResponse.h"
#include "WireCellIface/IFieldResponse.h"
#include "WireCellIface/IWaveform.h"
#include "WireCellUtil/FFTBestLength.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Testing.h"

WIRECELL_FACTORY(PlaneImpactResponse, WireCell::Gen::PlaneImpactResponse, WireCell::IPlaneImpactResponse,
                 WireCell::IConfigurable)

using namespace std;
using namespace WireCell;

const Waveform::compseq_t &Gen::ImpactResponse::spectrum()
{
    if (m_spectrum.size() != 0) {
        return m_spectrum;
    }
    else {
        m_spectrum = Waveform::dft(m_waveform);
        return m_spectrum;
    }
}

const Waveform::compseq_t &Gen::ImpactResponse::long_aux_spectrum()
{
    if (m_long_spectrum.size() != 0) {
        return m_long_spectrum;
    }
    else {
        m_long_spectrum = Waveform::dft(m_long_waveform);
        return m_long_spectrum;
    }
}

Gen::PlaneImpactResponse::PlaneImpactResponse(int plane_ident, size_t nbins, double tick)
  : m_frname("FieldResponse")
  , m_plane_ident(plane_ident)
  , m_nbins(nbins)
  , m_tick(tick)
  , l(Log::logger("geom"))
{
}

WireCell::Configuration Gen::PlaneImpactResponse::default_configuration() const
{
    Configuration cfg;
    // IFieldResponse component
    cfg["field_response"] = m_frname;
    // plane id to use to index into field response .plane()
    cfg["plane"] = 0;
    // names of IWaveforms interpreted as subsequent response
    // functions.
    cfg["short_responses"] = Json::arrayValue;
    cfg["overall_short_padding"] = 100 * units::us;
    cfg["long_responses"] = Json::arrayValue;
    cfg["long_padding"] = 1.5 * units::ms;
    // number of bins in impact response spectra
    cfg["nticks"] = 10000;
    // sample period of response waveforms
    cfg["tick"] = 0.5 * units::us;
    return cfg;
}

void Gen::PlaneImpactResponse::configure(const WireCell::Configuration &cfg)
{
    m_frname = get(cfg, "field_response", m_frname);
    m_plane_ident = get(cfg, "plane", m_plane_ident);

    m_short.clear();
    auto jfilts = cfg["short_responses"];
    if (!jfilts.isNull() and !jfilts.empty()) {
        for (auto jfn : jfilts) {
            auto tn = jfn.asString();
            m_short.push_back(tn);
        }
    }

    m_long.clear();
    auto jfilts1 = cfg["long_responses"];
    if (!jfilts1.isNull() and !jfilts1.empty()) {
        for (auto jfn : jfilts1) {
            auto tn = jfn.asString();
            m_long.push_back(tn);
        }
    }

    m_overall_short_padding = get(cfg, "overall_short_padding", m_overall_short_padding);
    m_long_padding = get(cfg, "long_padding", m_long_padding);

    m_nbins = (size_t) get(cfg, "nticks", (int) m_nbins);
    m_tick = get(cfg, "tick", m_tick);

    build_responses();
}

void Gen::PlaneImpactResponse::build_responses()
{
    auto ifr = Factory::find_tn<IFieldResponse>(m_frname);

    const size_t n_short_length = fft_best_length(m_overall_short_padding / m_tick);

    // build "short" response spectra
    WireCell::Waveform::compseq_t short_spec(n_short_length, Waveform::complex_t(1.0, 0.0));
    const size_t nshort = m_short.size();
    for (size_t ind = 0; ind < nshort; ++ind) {
        const auto &name = m_short[ind];
        auto iw = Factory::find_tn<IWaveform>(name);
        if (std::abs(iw->waveform_period() - m_tick) > 1 * units::ns) {
            l->critical("from {} got {} us sample period expected {} us", name, iw->waveform_period() / units::us,
                        m_tick / units::us);
            THROW(ValueError() << errmsg{"Tick mismatch in " + name});
        }
        auto wave = iw->waveform_samples();  // copy
        if (wave.size() != n_short_length) {
            l->debug(
                "PIR: short response {} has different number of samples ({}) "
                "than expected ({})",
                name, wave.size(), n_short_length);
            wave.resize(n_short_length, 0);
        }
        // note: we are ignoring waveform_start which will introduce
        // an arbitrary phase shift....
        auto spec = Waveform::dft(wave);
        for (size_t ibin = 0; ibin < n_short_length; ++ibin) {
            short_spec[ibin] *= spec[ibin];
        }
    }

    // build "long" response spectrum in time domain ...
    size_t n_long_length = fft_best_length(m_nbins);
    WireCell::Waveform::compseq_t long_spec(n_long_length, Waveform::complex_t(1.0, 0.0));
    const size_t nlong = m_long.size();
    for (size_t ind = 0; ind < nlong; ++ind) {
        const auto &name = m_long[ind];
        auto iw = Factory::find_tn<IWaveform>(name);
        if (std::abs(iw->waveform_period() - m_tick) > 1 * units::ns) {
            l->critical("from {} got {} us sample period expected {} us", name, iw->waveform_period() / units::us,
                        m_tick / units::us);
            THROW(ValueError() << errmsg{"Tick mismatch in " + name});
        }
        auto wave = iw->waveform_samples();  // copy
        if (wave.size() != n_long_length) {
            l->debug(
                "PIR: long response {} has different number of samples ({}) "
                "than expected ({})",
                name, wave.size(), n_long_length);
            wave.resize(n_long_length, 0);
        }
        // note: we are ignoring waveform_start which will introduce
        // an arbitrary phase shift....
        auto spec = Waveform::dft(wave);
        for (size_t ibin = 0; ibin < n_long_length; ++ibin) {
            long_spec[ibin] *= spec[ibin];
        }
    }
    WireCell::Waveform::realseq_t long_wf;
    if (nlong > 0) long_wf = Waveform::idft(long_spec);

    const auto &fr = ifr->field_response();
    const auto &pr = *fr.plane(m_plane_ident);
    const int npaths = pr.paths.size();

    // FIXME HUGE ASSUMPTIONS ABOUT ORGANIZATION OF UNDERLYING
    // FIELD RESPONSE DATA!!!
    //
    // Paths must be in increasing pitch with one impact position at
    // nearest wire and 5 more impact positions equally spaced and at
    // smaller pitch distances than the associated wire.  The final
    // impact position should be no further from the wire than 1/2
    // pitch.

    const int n_per = 6;  // fixme: assumption
    const int n_wires = npaths / n_per;
    const int n_wires_half = n_wires / 2;  // integer div
    // const int center_index = n_wires_half * n_per;

    /// FIXME: this assumes impact positions are on uniform grid!
    m_impact = std::abs(pr.paths[1].pitchpos - pr.paths[0].pitchpos);
    /// FIXME: this assumes paths are ordered by pitch
    m_half_extent = std::max(std::abs(pr.paths.front().pitchpos), std::abs(pr.paths.back().pitchpos));
    /// FIXME: this assumes detailed ordering of paths w/in one wire
    m_pitch = 2.0 * std::abs(pr.paths[n_per - 1].pitchpos - pr.paths[0].pitchpos);

    // native response time binning
    const int rawresp_size = pr.paths[0].current.size();
    const double rawresp_min = fr.tstart;
    const double rawresp_tick = fr.period;
    const double rawresp_max = rawresp_min + rawresp_size * rawresp_tick;
    Binning rawresp_bins(rawresp_size, rawresp_min, rawresp_max);

    // collect paths and index by wire and impact position.
    std::map<int, region_indices_t> wire_to_ind;
    for (int ipath = 0; ipath < npaths; ++ipath) {
        const Response::Schema::PathResponse &path = pr.paths[ipath];
        const int wirenum = int(ceil(path.pitchpos / pr.pitch));  // signed
        wire_to_ind[wirenum].push_back(ipath);

        // match response sampling to digi and zero-pad
        WireCell::Waveform::realseq_t wave(n_short_length, 0.0);
        for (int rind = 0; rind < rawresp_size; ++rind) {  // sample at fine bins of response function
            const double time = rawresp_bins.center(rind);

            // fixme: assumes field response appropriately centered
            const size_t bin = time / m_tick;

            if (bin >= n_short_length) {
                l->warn(
                    "PIR: out of bounds field response bin={}, ntbins={}, time={} "
                    "us, tick={} us",
                    bin, n_short_length, time / units::us, m_tick / units::us);
            }

            // Here we have sampled, instantaneous induced *current*
            // (in WCT system-of-units for current) due to a single
            // drifting electron from the field response function.
            const double induced_current = path.current[rind];

            // Integrate across the fine time bin to get the element
            // of induced *charge* over this bin.
            const double induced_charge = induced_current * rawresp_tick;

            // sum up over coarse ticks.
            wave[bin] += induced_charge;
        }
        WireCell::Waveform::compseq_t spec = Waveform::dft(wave);

        // Convolve with short responses
        if (nshort) {
            for (size_t find = 0; find < n_short_length; ++find) {
                spec[find] *= short_spec[find];
            }
        }
        Waveform::realseq_t wf = Waveform::idft(spec);
        wf.resize(m_nbins, 0);

        IImpactResponse::pointer ir = std::make_shared<Gen::ImpactResponse>(ipath, wf, m_overall_short_padding / m_tick,
                                                                            long_wf, m_long_padding / m_tick);
        m_ir.push_back(ir);
    }

    // apply symmetry.
    for (int irelwire = -n_wires_half; irelwire <= n_wires_half; ++irelwire) {
        auto direct = wire_to_ind[irelwire];
        auto other = wire_to_ind[-irelwire];

        std::vector<int> indices(direct.begin(), direct.end());
        for (auto it = other.rbegin() + 1; it != other.rend(); ++it) {
            indices.push_back(*it);
        }
        m_bywire.push_back(indices);
    }
}

Gen::PlaneImpactResponse::~PlaneImpactResponse() {}

// const Response::Schema::PlaneResponse& PlaneImpactResponse::plane_response()
// const
// {
//     return *m_fr.plane(m_plane_ident);
// }

std::pair<int, int> Gen::PlaneImpactResponse::closest_wire_impact(double relpitch) const
{
    const int center_wire = nwires() / 2;

    const int relwire = int(round(relpitch / m_pitch));
    const int wire_index = center_wire + relwire;

    const double remainder_pitch = relpitch - relwire * m_pitch;
    const int impact_index = int(round(remainder_pitch / m_impact)) + nimp_per_wire() / 2;

    return std::make_pair(wire_index, impact_index);
}

IImpactResponse::pointer Gen::PlaneImpactResponse::closest(double relpitch) const
{
    if (relpitch < -m_half_extent || relpitch > m_half_extent) {
        return nullptr;
    }
    std::pair<int, int> wi = closest_wire_impact(relpitch);
    if (wi.first < 0 || wi.first >= (int) m_bywire.size()) {
        l->debug("PIR: closest relative pitch:{} outside of wire range: {}", relpitch, wi.first);
        return nullptr;
    }
    const std::vector<int> &region = m_bywire[wi.first];
    if (wi.second < 0 || wi.second >= (int) region.size()) {
        l->debug("PIR: relative pitch:{} outside of impact range: {}", relpitch, wi.second);
        return nullptr;
    }
    int irind = region[wi.second];
    if (irind < 0 || irind > (int) m_ir.size()) {
        l->debug("PIR: relative pitch:{} no impact response for region: {}", relpitch, irind);
        return nullptr;
    }

    return m_ir[irind];
}

TwoImpactResponses Gen::PlaneImpactResponse::bounded(double relpitch) const
{
    if (relpitch < -m_half_extent || relpitch > m_half_extent) {
        return TwoImpactResponses(nullptr, nullptr);
    }

    std::pair<int, int> wi = closest_wire_impact(relpitch);

    auto region = m_bywire[wi.first];
    if (wi.second == 0) {
        return std::make_pair(m_ir[region[0]], m_ir[region[1]]);
    }
    if (wi.second == (int) region.size() - 1) {
        return std::make_pair(m_ir[region[wi.second - 1]], m_ir[region[wi.second]]);
    }

    const double absimpact = m_half_extent + relpitch - wi.first * m_pitch;
    const double sign = absimpact - wi.second * m_impact;

    if (sign > 0) {
        return TwoImpactResponses(m_ir[region[wi.second]], m_ir[region[wi.second + 1]]);
    }
    return TwoImpactResponses(m_ir[region[wi.second - 1]], m_ir[region[wi.second]]);
}
