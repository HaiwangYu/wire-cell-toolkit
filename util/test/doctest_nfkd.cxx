#include "WireCellUtil/NFKD.h"
#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/Logging.h"

// #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "WireCellUtil/doctest.h"

#include <vector>

using spdlog::debug;
using namespace WireCell;

using point_type = std::vector<double>;
using points_t = std::vector<point_type>;

TEST_CASE("nfkd dynamic")
{
    const size_t dimension = 3;
    points_t points = {
        // x    y    z
        {1.0, 2.0, 1.0},
        {1.0, 1.0, 4.0},
        {1.0, 3.0, 1.0}
    };
    // Note, the disjoint range used inside the k-d tree REQUIRES stable
    // iterators.  vector WILL NOT work unless it is never modified after the
    // disjoint range is filled.
    // 
    // std::vector<points_t> vpoints = {points};
    std::map<size_t, points_t> vpoints = { {0, points} };
        
    debug("nfkd: making k-d tree");
    NFKD::Tree<points_t> kd(dimension, vpoints[0].begin(), vpoints[0].end());

    REQUIRE(kd.points().size() == 3);
    debug("nfkd: {} point calls", kd.point_calls());

    CHECK(kd.points().at(0) == points[0]);
    CHECK(kd.points().at(1) == points[1]);
    CHECK(kd.points().at(2) == points[2]);
    CHECK_THROWS_AS(kd.points().at(3), std::out_of_range); // throws

    for (size_t count = 0; count < 3; ++count)
    {
        debug("nfkd: radius query #{}", count);
        point_type pt = {1.0, 2.0, 1.0};
        auto res = kd.radius(0.01, pt);
        CHECK(res.size() == 1);
        for (const auto& [it,dist] : res) {
            const point_type& pt = *it;
            debug("rad: #{} point=({},{},{}), dist={}, calls={}",
                  count, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }

    debug("nfkd: appending");
    kd.append(points);

    auto& kdpoints = kd.points();

    REQUIRE(kdpoints.size() == 6);

    for (size_t kpt=0; kpt<6; ++kpt) {
        const size_t ipt = 0;   // points reused
        for (size_t dim=0; dim<3; ++dim) {
            const double kele = kdpoints.at(kpt)[dim];
            const double iele = points.at(ipt)[dim];
            debug("kpt={} dim={} {} =?= {}", kpt, dim, kele, iele);
            // CHECK(kele == iele);
        }
    }

    for (size_t N = 1; N <= 6; ++N) {
        debug("nfkd: knn={} query", N);
        point_type pt = {0,0,0};
        auto res = kd.knn(N, pt);
        CHECK(res.size() == N);
        for (const auto& [it,dist] : res) {
            const point_type& pt = *it;
            debug("knn: N={} point=({},{},{}), dist={}, calls={}",
                  N, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }
}

TEST_CASE("nfkd static two")
{
    const size_t dimension = 3;
    points_t points = {
        // x    y    z
        {1.0, 2.0, 1.0},
        {1.0, 1.0, 4.0},
        {1.0, 3.0, 1.0}
    };
    std::vector<points_t> vpoints = {points, points};
        
    debug("nfkd: making k-d tree");
    NFKD::Tree<points_t> kd(dimension, vpoints );

    REQUIRE(kd.points().size() == 6);
    debug("nfkd: {} point calls", kd.point_calls());

    CHECK(kd.points().at(0) == points[0]);
    CHECK(kd.points().at(1) == points[1]);
    CHECK(kd.points().at(2) == points[2]);
    CHECK(kd.points().at(3) == points[0]);
    CHECK(kd.points().at(4) == points[1]);
    CHECK(kd.points().at(5) == points[2]);
    CHECK_THROWS_AS(kd.points().at(6), std::out_of_range); // throws

    for (size_t count = 0; count < 3; ++count)
    {
        debug("nfkd: radius query #{}", count);
        point_type pt = {1.0, 2.0, 1.0};
        auto res = kd.radius(0.01, pt);
        CHECK(res.size() == 2);
        for (const auto& [it,dist] : res) {
            const point_type& pt = *it;
            debug("rad: #{} point=({},{},{}), dist={}, calls={}",
                  count, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }

    auto& kdpoints = kd.points();

    REQUIRE(kdpoints.size() == 6);

    for (size_t kpt=0; kpt<6; ++kpt) {
        const size_t ipt = 0;   // points reused
        for (size_t dim=0; dim<3; ++dim) {
            const double kele = kdpoints.at(kpt)[dim];
            const double iele = points.at(ipt)[dim];
            debug("kpt={} dim={} {} =?= {}", kpt, dim, kele, iele);
            // CHECK(kele == iele);
        }
    }

    for (size_t N = 1; N <= 6; ++N) {
        debug("nfkd: knn={} query", N);
        point_type pt = {0,0,0};
        auto res = kd.knn(N, pt);
        CHECK(res.size() == N);
        for (const auto& [it,dist] : res) {
            const point_type& pt = *it;
            debug("knn: N={} point=({},{},{}), dist={}, calls={}",
                  N, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }
}

TEST_CASE("nfkd static")
{
    const size_t dimension = 3;

    points_t points = {
        // x    y    z
        {1.0, 2.0, 1.0},
        {1.0, 1.0, 4.0},
        {1.0, 3.0, 1.0}
    };
    std::vector<points_t> vpoints = {points};
        
    debug("nfkd: making k-d tree");
    NFKD::Tree<points_t, NFKD::IndexStatic> kd(dimension, vpoints );

    CHECK(kd.points().size() == 3);
    debug("nfkd: {} point calls", kd.point_calls());

    CHECK(kd.points().at(0) == points[0]);
    CHECK(kd.points().at(1) == points[1]);
    CHECK(kd.points().at(2) == points[2]);
    CHECK_THROWS_AS(kd.points().at(3), std::out_of_range); // throws

    for (size_t count = 0; count < 3; ++count)
    {
        debug("nfkd: radius query #{}", count);
        point_type pt = {1.0, 2.0, 1.0};
        auto rad = kd.radius(0.01, pt);
        CHECK(rad.size() == 1);
        for (const auto& [it,dist] : rad) {
            const point_type& pt = *it;
            debug("rad: #{} point=({},{},{}), dist={}, calls={}",
                  count, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }

    for (size_t N = 1; N <= 3; ++N) {
        debug("nfkd: knn={} query", N);
        point_type pt = {0,0,0};
        auto knn = kd.knn(N, pt);
        CHECK(knn.size() == N);
        for (const auto& [it,dist] : knn) {
            const point_type& pt = *it;
            debug("knn: N={} point=({},{},{}), dist={}, calls={}",
                  N, pt[0], pt[1], pt[2], dist, kd.point_calls());
        }
    }

    {
        const auto& ckd = kd;
        point_type pt = {1.0, 2.0, 1.0};
        auto rad = ckd.radius(0.01, pt);
        CHECK(rad.size() == 1);
    }    

}
