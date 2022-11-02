
#include "WireCellImg/TestProjection2D.h"
#include "WireCellImg/CSGraph.h"
#include "WireCellImg/Projection2D.h"

#include "WireCellAux/SimpleCluster.h"

#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Logging.h"
#include "WireCellUtil/String.h"
#include "WireCellUtil/Rectangles.h"

#include <iterator>
#include <chrono>

WIRECELL_FACTORY(TestProjection2D, WireCell::Img::TestProjection2D,
                 WireCell::INamed,
                 WireCell::IClusterFilter, WireCell::IConfigurable)

using namespace WireCell;
using namespace WireCell::Img;
using namespace WireCell::Img::CS;
using namespace WireCell::Img::Projection2D;

Img::TestProjection2D::TestProjection2D()
    : Aux::Logger("TestProjection2D", "img")
{
}

Img::TestProjection2D::~TestProjection2D() {}

WireCell::Configuration Img::TestProjection2D::default_configuration() const
{
    WireCell::Configuration cfg;
    cfg["compare_brute_force"] = m_compare_brute_force;
    cfg["compare_rectangle"] = m_compare_rectangle;
    cfg["verbose"] = m_verbose;

    return cfg;
}

void Img::TestProjection2D::configure(const WireCell::Configuration& cfg)
{
    m_compare_brute_force = get<bool>(cfg,"compare_brute_force", m_compare_brute_force);
    m_compare_rectangle = get<bool>(cfg,"compare_rectangle", m_compare_rectangle);
    m_verbose = get<bool>(cfg,"verbose", m_verbose);
    log->debug("m_compare_brute_force: {}", m_compare_brute_force);
    log->debug("m_compare_rectangle: {}", m_compare_rectangle);
    log->debug("m_verbose: {}", m_verbose);
}

static void dump_cg(const cluster_graph_t& cg, Log::logptr_t& log)
{
    size_t mcount{0}, bcount{0};
    value_t bval;
    for (const auto& vtx : mir(boost::vertices(cg))) {
        const auto& node = cg[vtx];
        if (node.code() == 'b') {
            const auto iblob = get<blob_t>(node.ptr);
            bval += value_t(iblob->value(), iblob->uncertainty());
            ++bcount;
            continue;
        }
        if (node.code() == 'm') {
            const auto imeas = get<meas_t>(node.ptr);
            ++mcount;
        }
    }
    log->debug("cluster graph: vertices={} edges={} #blob={} bval={} #meas={}",
               boost::num_vertices(cg), boost::num_edges(cg),
               bcount, bval, mcount);
}

namespace {

    // run in keep mode if removal_mode is false
    cluster_graph_t remove_blobs(const cluster_graph_t& cg, std::unordered_set<cluster_vertex_t> blobs, bool removal_mode = true)
    {
        cluster_graph_t cg_out;
        
        size_t nblobs = 0;
        std::unordered_map<cluster_vertex_t, cluster_vertex_t> old2new;
        for (const auto& vtx : mir(boost::vertices(cg))) {
            const auto& node = cg[vtx];
            if (node.code() == 'b') {
                bool removal = blobs.find(vtx) != blobs.end();
                if (!removal_mode) removal =  !removal;
                if (!removal) {
                    ++nblobs;
                    old2new[vtx] = boost::add_vertex(node, cg_out);
                }
            } else {
                old2new[vtx] = boost::add_vertex(node, cg_out);
            }
        }
        
        std::cout << "nblobs: " << nblobs << std::endl;
        std::cout << "boost::num_vertices(cg_out): " << boost::num_vertices(cg_out) << std::endl;

        for (auto edge : mir(boost::edges(cg))) {
            auto old_tail = boost::source(edge, cg);
            auto old_head = boost::target(edge, cg);

            auto old_tit = old2new.find(old_tail);
            if (old_tit == old2new.end()) {
                continue;
            }
            auto old_hit = old2new.find(old_head);
            if (old_hit == old2new.end()) {
                continue;
            }
            boost::add_edge(old_tit->second, old_hit->second, cg_out);
        }
        std::cout << "boost::num_vertices(cg_out): " << boost::num_vertices(cg_out) << std::endl;
        return cg_out;
    }
}

bool Img::TestProjection2D::operator()(const input_pointer& in, output_pointer& out)
{
    out = nullptr;
    if (!in) {
        log->debug("EOS");
        return true;
    }
    const auto in_graph = in->graph();
    dump_cg(in_graph, log);
    auto time_start = std::chrono::high_resolution_clock::now();
    auto time_stop = std::chrono::high_resolution_clock::now();
    auto time_duration = std::chrono::duration_cast<std::chrono::seconds>(time_stop - time_start);

    // Geom. clustering (connected blobs)
    time_start = std::chrono::high_resolution_clock::now();
    auto id2cluster = get_geom_clusters(in_graph);
    time_stop = std::chrono::high_resolution_clock::now();
    time_duration = std::chrono::duration_cast<std::chrono::seconds>(time_stop - time_start);
    log->debug("Geom. clustering: {} sec", time_duration.count());
    log->debug("found: {} blob geom clusters", id2cluster.size());

    // Make Projection2D and build Rectangles
    time_start = std::chrono::high_resolution_clock::now();
    std::unordered_map<int, layer_projection_map_t> id2lproj;
    std::unordered_set<WirePlaneLayer_t> layers;
    using rect_t = Rectangles<int, int>;
    using layer_rects_t = std::unordered_map<WirePlaneLayer_t, rect_t>;
    layer_rects_t lrecs;
    for (auto ic : id2cluster) {
        int id = ic.first;
        auto cluster = ic.second;
        // TODO zero-copy?
        id2lproj[id] = get_2D_projection(in_graph, cluster);
        for (auto lproj : id2lproj[id]) {
            auto layer = lproj.first;
            auto proj = lproj.second;
            projection_bound_t bound = proj.m_bound;
            lrecs[layer].add(
                std::get<0>(bound),
                std::get<1>(bound)+1,
                std::get<2>(bound),
                std::get<3>(bound)+1,
                id);
            layers.insert(layer);
            // log->debug("{{{},{}}} => {}", id, layer, dump(proj));
            const std::string aname = String::format("proj2d_%d.tar.gz", (int)layer);
            // write(proj, aname);
        }
        // exit(42);
    }
    time_stop = std::chrono::high_resolution_clock::now();
    time_duration = std::chrono::duration_cast<std::chrono::seconds>(time_stop - time_start);
    log->debug("Make Projection2D and build Rectangles: {} sec", time_duration.count());

    std::map<std::string, int> counters = {
        {"ncomp", 0},
        {"n1", 0}
    };


    // for debugging
    std::unordered_set<cluster_vertex_t> bs_keep{758, 634};

    if (m_compare_rectangle) {
        time_start = std::chrono::high_resolution_clock::now();
        // std::unordered_set<int> check_ids = {70,71};

        using checked_pairs_t = std::set< std::pair<int, int> >;
        using layer_checked_pairs_t = std::unordered_map< WirePlaneLayer_t, checked_pairs_t>;
        layer_checked_pairs_t layer_checked_pairs;
        [&] {
        for (auto layer : layers) {
            for (auto ref_ilp : id2lproj) {
                auto ref_id = ref_ilp.first;
                auto& ref_proj = ref_ilp.second[layer];
                // if (check_ids.find(ref_id)!=check_ids.end()) {
                //     log->debug("ref: {{{},{}}} => {}", ref_id, layer, dump(ref_proj));
                // }
                projection_bound_t ref_bound = ref_proj.m_bound;
                auto overlap_regions = lrecs[layer].intersection(
                    {std::get<0>(ref_bound),std::get<1>(ref_bound)+1},
                    {std::get<2>(ref_bound),std::get<3>(ref_bound)+1}
                );
                // if (check_ids.find(ref_id)!=check_ids.end()) {
                //     for(const auto& [rec, ids] : overlap_regions) {
                //         for (const auto& tar_id : ids) {
                //             log->debug("overlap_regions ids: {}", tar_id);
                //         }
                //     }
                // }
                for(const auto& [rec, ids] : overlap_regions) {
                    for (const auto& tar_id : ids) {
                        if (tar_id == ref_id) continue;
                        if (layer_checked_pairs[layer].find({ref_id, tar_id}) != layer_checked_pairs[layer].end()) continue;
                        counters["ncomp"] += 1;
                        auto& tar_proj = id2lproj[tar_id][layer];
                        int coverage = judge_coverage(ref_proj, tar_proj);
                        if (coverage != 0) {
                            counters["n1"] += 1;
                            if (m_verbose) {
                                if (id2cluster[ref_id].size() > 1) {
                                    log->debug("ref: {{{},{}}} => {}", ref_id, layer, dump(ref_proj,true));
                                    log->debug("tar: {{{},{}}} => {}", tar_id, layer, dump(tar_proj,true));
                                    std::cout << ref_id << " {";
                                    for (auto& b : id2cluster[ref_id]) {
                                        std::cout << b << " ";
                                        bs_keep.insert(b);
                                    }
                                    std::cout << "} -> ";
                                    std::cout << tar_id << " {";
                                    for (auto& b : id2cluster[tar_id]) {
                                        std::cout << b << " ";
                                        bs_keep.insert(b);
                                    }
                                    std::cout << "} \n";
                                    return;
                                }
                            }
                        }
                        layer_checked_pairs[layer].insert({ref_id, tar_id});
                    }
                }
            }
        }
        }();
        time_stop = std::chrono::high_resolution_clock::now();
        time_duration = std::chrono::duration_cast<std::chrono::seconds>(time_stop - time_start);
        log->debug("judge_coverage: {} sec", time_duration.count());
        for (auto c : counters) {
            log->debug("{} : {} ", c.first, c.second);
        }
    }

    if (m_compare_brute_force) {
        counters["ncomp"] = 0;
        counters["n1"] = 0;
        // layer_checked_pairs.clear();
        time_start = std::chrono::high_resolution_clock::now();
        for (auto layer : layers) {
            for (auto ref_ilp : id2lproj) {
                auto ref_id = ref_ilp.first;
                auto& ref_proj = ref_ilp.second[layer];
                for (auto tar_ilp : id2lproj) {
                    auto tar_id = tar_ilp.first;
                    if (tar_id == ref_id) continue;
                    // if (layer_checked_pairs[layer].find({ref_id, tar_id}) == layer_checked_pairs[layer].end()) continue;
                    auto& tar_proj = tar_ilp.second[layer];
                    counters["ncomp"] += 1;
                    int coverage = judge_coverage(ref_proj, tar_proj);
                    if (coverage != 0) {
                        counters["n1"] += 1;
                        if (m_verbose) {
                            log->debug("ref: {{{},{}}} => {}", ref_id, layer, dump(ref_proj));
                            log->debug("tar: {{{},{}}} => {}", tar_id, layer, dump(tar_proj));
                        }
                    }
                    // layer_checked_pairs[layer].insert({ref_id, tar_id});
                }
            }
        }
        time_stop = std::chrono::high_resolution_clock::now();
        time_duration = std::chrono::duration_cast<std::chrono::seconds>(time_stop - time_start);
        log->debug("judge_coverage: {} sec", time_duration.count());
        for (auto c : counters) {
            log->debug("{} : {} ", c.first, c.second);
        }
    }

    auto out_graph = remove_blobs(in_graph,bs_keep,false);
    // auto& out_graph = in_graph;
    dump_cg(out_graph, log);
    out = std::make_shared<Aux::SimpleCluster>(out_graph, in->ident());
    return true;
}


