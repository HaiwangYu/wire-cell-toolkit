#include "WireCellUtil/PointTree.h"
#include "WireCellUtil/PointTesting.h"

#include "WireCellUtil/doctest.h"

#include "WireCellUtil/Logging.h"

#include <unordered_map>

using namespace WireCell;
using namespace WireCell::PointTesting;
using namespace WireCell::PointCloud;
using namespace WireCell::PointCloud::Tree;
using spdlog::debug;

using node_ptr = std::unique_ptr<Points::node_t>;


TEST_CASE("point tree scope")
{
    Scope s;
    CHECK(s.pcname == "");
    CHECK(s.coords.empty());
    CHECK(s.depth == 0);

    Scope s0{ "pcname", {"x","y","z"}, 0};
    Scope s1{ "pcname", {"x","y","z"}, 1};
    Scope s2{ "PCNAME", {"x","y","z"}, 0};
    Scope s3{ "pcname", {"X","Y","Z"}, 0};
    Scope sc{ "pcname", {"x","y","z"}, 0};

    CHECK(s0 == s0);
    CHECK(s0 == sc);
    CHECK(s0 != s1);
    CHECK(s0 != s2);
    CHECK(s0 != s3);

    CHECK(s0.hash() == sc.hash());
    CHECK(s0.hash() != s1.hash());
    CHECK(s0.hash() != s2.hash());
    CHECK(s0.hash() != s3.hash());

    std::unordered_map<Scope, size_t> m;
    m[s1] = 1;
    m[s2] = 2;
    CHECK(m[s0] == 0);          // spans default
    CHECK(m[s1] == 1);
    CHECK(m[s2] == 2);

}

TEST_CASE("point tree points empty")
{
    Points p;
    CHECK(p.node() == nullptr);
    auto& lpcs = p.local_pcs();
    CHECK(lpcs.empty());

    Scope s;
    const auto& dds = p.scoped_pc(s);
    CHECK(dds.datasets().empty());
    CHECK(dds.npoints() == 0);

    const auto& kd = p.scoped_kd(s);
    const auto& dds2 = kd.pointclouds();
    CHECK(dds2.datasets().empty());
    CHECK(dds2.npoints() == 0);
    CHECK(&dds == &dds2);
}

static
Points::node_ptr make_simple_pctree()
{
    Points::node_ptr root = std::make_unique<Points::node_t>();

    // Insert a child with a set of named points clouds with one point
    // cloud from a track.
    root->insert(Points({ {"3d", make_janky_track()} }));

    // Ibid from a different track
    root->insert(Points({ {"3d", make_janky_track(
                        Ray(Point(-1, 2, 3), Point(1, -2, -3)))} }));

    return root;
}

TEST_CASE("point tree real")
{
    auto root = make_simple_pctree();
    CHECK(root.get());

    auto& rval = root->value;

    CHECK(root->children().size() == 2);
    CHECK(root.get() == rval.node());
    CHECK(rval.local_pcs().empty());
    
    {
        auto& cval = *(root->child_values().begin());
        const auto& pcs = cval.local_pcs();
        CHECK(pcs.size() > 0);
        for (const auto& [key,val] : pcs) {
            debug("child has pc named \"{}\" with {} points", key, val.size_major());
        }
        const auto& pc3d = pcs.at("3d");
        debug("got child PC named \"3d\" with {} points", pc3d.size_major());
        CHECK(pc3d.size_major() > 0);
        CHECK(pc3d.has("x"));
        CHECK(pc3d.has("y"));
        CHECK(pc3d.has("z"));
        CHECK(pc3d.has("q"));
    }

    Scope scope{ "3d", {"x","y","z"}};

    const auto& pc3d = rval.scoped_pc(scope);
    CHECK(pc3d.datasets().size() == 2);

    const auto& kd = rval.scoped_kd(scope);
    CHECK(&kd.pointclouds() == &pc3d);

    auto knn = kd.knn(6, {0,0,0});
    for (auto [ind,dist] : knn) {
        // fixme: warning this per index lookup is probably expensive.
        auto [dsnum,dsind] = pc3d.index(ind);
        const Dataset& ds = pc3d.datasets()[dsnum];
        selection_t sel = ds.selection(scope.coords);
        debug("knn: {}=({},{}): {}", ind, dsnum, dsind, dist);
              // sel[0].get().element<double>(dsind),
              // sel[1].get().element<double>(dsind),
              // sel[2].get().element<double>(dsind),
              // sel[3].get().element<double>(dsind));
    }
    CHECK(knn.size() == 6);


    auto rad = kd.radius(.001, {0,0,0});
    for (auto [ind,dist] : rad) {
        auto [dsnum,dsind] = pc3d.index(ind);
        const Dataset& ds = pc3d.datasets()[dsnum];
        selection_t sel = ds.selection(scope.coords);
        debug("rad: {}=({},{}): {}", ind, dsnum, dsind, dist);
        // debug("rad: {}: {} = ({},{},{},{})", ind, dist,
        //       sel[0].get().element<double>(dsind),
        //       sel[1].get().element<double>(dsind),
        //       sel[2].get().element<double>(dsind),
        //       sel[3].get().element<double>(dsind));
    }
    CHECK(rad.size() == 6);

}