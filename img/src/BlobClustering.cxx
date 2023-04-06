#include "WireCellImg/BlobClustering.h"

#include "WireCellAux/SimpleCluster.h"

#include "WireCellUtil/RayClustering.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/String.h"

#include "WireCellUtil/IndexedGraph.h"

// #include <boost/graph/graphviz.hpp>

WIRECELL_FACTORY(BlobClustering, WireCell::Img::BlobClustering,
                 WireCell::INamed,
                 WireCell::IClustering, WireCell::IConfigurable)

using namespace WireCell;

Img::BlobClustering::BlobClustering()
    : Aux::Logger("BlobClustering", "img")
{
}
Img::BlobClustering::~BlobClustering() {}

void Img::BlobClustering::configure(const WireCell::Configuration& cfg)
{
    // m_spans = get(cfg, "spans", m_spans);
    m_policy = get(cfg, "policy", m_policy);
    if (m_policy != "simple" and m_policy != "uboone") {
        THROW(ValueError() << errmsg{String::format("policy %s not implemented!", m_policy)});
    }
}

WireCell::Configuration Img::BlobClustering::default_configuration() const
{
    Configuration cfg;

    // maxgap = spans*span of current slice.  If next slice starts
    // later than this relative to current slice then a gap exists.
    // cfg["spans"] = m_spans;
    cfg["policy"] = m_policy;

    return cfg;
}


// add a slice to the graph.
static
void add_slice(cluster_indexed_graph_t& grind,
               const ISlice::pointer& islice)
{
    if (grind.has(islice)) {
        return;
    }

    for (const auto& ichv : islice->activity()) {
        const IChannel::pointer ich = ichv.first;
        if (grind.has(ich)) {
            continue;
        }
        for (const auto& iwire : ich->wires()) {
            grind.edge(ich, iwire);
        }
    }
}

// add blobs to the graph
static 
void add_blobs(cluster_indexed_graph_t& grind,
               const IBlob::vector& iblobs)
{
    for (const auto& iblob : iblobs) {
        auto islice = iblob->slice();
        add_slice(grind, islice);
        grind.edge(islice, iblob);

        auto iface = iblob->face();
        auto wire_planes = iface->planes();

        const auto& shape = iblob->shape();
        for (const auto& strip : shape.strips()) {
            // FIXME: need a way to encode this convention!
            // For now, it requires collusion.  Don't impeach me.
            const int num_nonplane_layers = 2;
            int iplane = strip.layer - num_nonplane_layers;
            if (iplane < 0) {
                continue;
            }
            const auto& wires = wire_planes[iplane]->wires();
            for (int wip = strip.bounds.first; wip < strip.bounds.second and wip < int(wires.size()); ++wip) {
                auto iwire = wires[wip];
                grind.edge(iblob, iwire);
            }
        }
    }
}


// return the time between two blob sets relative to the slice span of
// the first.
static
int rel_time_diff(const IBlobSet::pointer& one,
                     const IBlobSet::pointer& two)
{
    const auto here = one->slice();

    return std::round((two->slice()->start() - here->start())/here->span());
}


static
IBlobSet::vector::iterator
intern_one(cluster_indexed_graph_t& grind,
           IBlobSet::vector::iterator beg,
           IBlobSet::vector::iterator end,
           std::string policy)
{
    add_blobs(grind, (*beg)->blobs());

    auto next = beg+1;

    if (next == end) {
        return end;
    }

    if (policy != "simple" and policy != "uboone") {
        THROW(ValueError() << errmsg{String::format("policy %s not implemented!", policy)});
    }

    // uboone policy
    int max_rel_diff = 2;
    std::map<int, RayGrid::grid_index_t> map_gap_tol = {
        {1, 2},
        {2, 1}
    };

    if (policy == "simple") {
        max_rel_diff = 1;
        map_gap_tol = {{1,0}};
    }

    // overlap + tolerance
    struct TolerantVisitor {
        RayGrid::grid_index_t tolerance{0};
        bool verbose{false};
        RayGrid::blobvec_t operator()(const RayGrid::blobref_t& blob, const RayGrid::blobproj_t& proj, RayGrid::layer_index_t layer)
        {
            return overlap(blob, proj, layer, tolerance, verbose);
        }
    };

    for (auto test = next; test != end and rel_time_diff(*beg, *test) <= max_rel_diff; ++test) {
        int rel_diff = rel_time_diff(*beg, *test);
        // TODO: remove debug info
        // int btick = (*beg)->slice()->start()/((*beg)->slice()->span())*4;
        // int ttick = (*test)->slice()->start()/((*test)->slice()->span())*4;
        // if(btick == 664 && ttick == 668) {
        //     std::cout << "btick: " << btick << ", ttick: " << ttick << std::endl;
        // }
        if (map_gap_tol.find(rel_diff)==map_gap_tol.end()) continue;

        // handle each face separately faces
        IBlob::vector iblobs1 = (*test)->blobs();
        IBlob::vector iblobs2 = (*beg)->blobs();

        RayGrid::blobs_t blobs1 = (*test)->shapes();
        RayGrid::blobs_t blobs2 = (*beg)->shapes();
        // TODO: remove debug info
        // if(btick == 664 && ttick == 668) {
        //     std::cout << "tolerence: " << map_gap_tol[rel_diff] << std::endl;
        //     std::cout << "beg:\n";
        //     for (size_t i=0; i< iblobs2.size(); ++i) {
        //         std::cout << iblobs2[i].get()->ident() << std::endl;
        //         std::cout << blobs2[i].as_string() << std::endl;
        //     }
        //     std::cout << "test:\n";
        //     for (size_t i=0; i< iblobs1.size(); ++i) {
        //         std::cout << iblobs1[i].get()->ident() << std::endl;
        //         std::cout << blobs1[i].as_string() << std::endl;
        //     }
        // }

        const auto beg1 = blobs1.begin();
        const auto beg2 = blobs2.begin();

        auto assoc = [&](RayGrid::blobref_t& a, RayGrid::blobref_t& b) {
            int an = a - beg1;
            int bn = b - beg2;
            grind.edge(iblobs1[an], iblobs2[bn]);
            // TODO: remove debug info
            // if(btick == 664 && ttick == 668) {
            //     std::cout << iblobs1[an].get()->ident() << " --- " << iblobs2[bn].get()->ident() << std::endl;
            // }
        };
        bool verbose = false;
        // TODO: remove debug info
        // if (btick == 664 && ttick == 668) {
        //     verbose = true;
        // }
        TolerantVisitor tv{map_gap_tol[rel_diff], verbose};
        // RayGrid::associate(blobs1, blobs2, assoc, tv);
        RayGrid::associate(blobs1, blobs2, assoc, tv);
    }

    return next;
}

void Img::BlobClustering::flush(output_queue& clusters)
{
    // 1) sort cache
    std::sort(m_cache.begin(), m_cache.end(), [](const auto& a, const auto& b) {
        return a->slice()->start() < b->slice()->start();
    });

    cluster_indexed_graph_t grind;

    // 2) intern the cached blob sets
    auto bsit = m_cache.begin();
    auto bend = m_cache.end();
    while (bsit != bend) {
        bsit = intern_one(grind, bsit, bend, m_policy);
    }

    // 3) pack and clear
    clusters.push_back(std::make_shared<Aux::SimpleCluster>(grind.graph(), cur_ident()));
    m_cache.clear();
}


// dig out the frame ID
static int frame_ident(const IBlobSet::pointer& bs)
{
    return bs->slice()->frame()->ident();
}

int Img::BlobClustering::cur_ident() const
{
    if (m_cache.empty()) return false;
    return frame_ident(m_cache.front());
}

// Determine if we have a BlobSet from a fresh frame
bool Img::BlobClustering::new_frame(const input_pointer& newbs) const
{
    if (m_cache.empty()) return false;
    return frame_ident(newbs) != frame_ident(m_cache[0]);
}

bool Img::BlobClustering::operator()(const input_pointer& blobset,
                                     output_queue& clusters)
{
    // wrt to last blobset, the input may be:
    // - EOS
    // - empty and same frame
    // - empty and new frame
    // - not empty and same frame
    // - not empty and new frame
    
    if (!blobset) {  // eos
        flush(clusters);
        log->debug("flush {} clusters + EOS on EOS at call={}",
                   clusters.size(), m_count);
        for (const auto& cl : clusters) {
            const auto& gr = cl->graph();
            
            log->debug("call={} cluster={} nvertices={} nedges={}",
                       m_count,
                       cl->ident(), boost::num_vertices(gr),
                       boost::num_edges(gr));
        }
        clusters.push_back(nullptr);  // forward eos
        ++m_count;
        return true;
    }

    if (new_frame(blobset)) {
        flush(clusters);
    }

    m_cache.push_back(blobset);
    ++m_count;
    return true;
}
