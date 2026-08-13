// Regression tests: TrackFitting::clear_segments() multi-event lifetime invariants.
//
// Root cause: TaggerCheckSTM owns a mutable TrackFitting that persists across
// events as a member of the long-lived MABC pipeline.  When event N's track
// fitting ran, add_segment() set m_grouping to event N's Grouping.
// clear_segments() was called between events but did NOT reset m_grouping.
// Event N's Grouping was freed when operator() returned.  Event N+1's
// add_segment() found m_grouping != nullptr, skipped BuildGeometry(), then
// called m_grouping->get_anode() on freed memory → SIGSEGV.
//
// Validated by PBS integration gate before this file was written:
//   Pre-fix : job 7439444 SEGFAULT at E=39→E=40 boundary
//   Post-fix: jobs 7445041, 7445462, 7446816 PASS (50-event gate, bit-identical H5)
//
// TEST COVERAGE:
//   TC-1  Postcondition of clear_segments() from default (null) state.
//         Passes on pre-fix 0869668 — vacuously true since m_grouping starts null.
//         Retained as a sanity check for the default state invariant.
//
//   TC-2  Repeated clear_segments() from default state — idempotency.
//         Same caveat as TC-1; passes on pre-fix.
//
//   TC-3  [TRUE REGRESSION] clear_segments() resets m_grouping that was set
//         by preload_clusters().
//         FAILS on pre-fix 0869668 (m_grouping not reset → still non-null).
//         PASSES on post-fix 983495f (m_grouping = nullptr added).
//         Uses a bare Facade::Grouping/Cluster tree (no anodes).
//         preload_clusters() sets m_grouping BEFORE calling BuildGeometry();
//         BuildGeometry() → fill_cache() throws "anode is null" (m_anodes empty
//         check fires before wpids are computed).  REQUIRE_THROWS catches this;
//         m_grouping is non-null post-throw.  clear_segments() is then the pin.
//         Confirmed empirically (PBS job 7448692, 2026-08-13).
//
// global_rb_map invariant: verifying that clear_segments() also clears
// global_rb_map requires populating it first, which needs blobs with real
// charge/readout data (fill_global_rb_map iterates grouping->children() down
// to blob wire/time data).  No public size accessor exists.  A small const
// accessor global_rb_map_size() would enable this check without exposing
// implementation detail; that design tradeoff needs WCT-team review before
// adding.  The stale-pointer risk from global_rb_map is covered by the PBS
// integration gate (50-event persistent streaming, job 7446816).
//
// search_other_tracks path (Part B of the fix): the m_track_fitter.clear_segments()
// call added at the top of TaggerCheckSTM::search_other_tracks() cannot be
// unit-tested without a fully-configured TaggerCheckSTM (IDetectorVolumes,
// IAnodePlane, real cluster data).  That path is covered by the E39→E40 PBS
// regression gate (job 7445041).

#include "WireCellUtil/doctest.h"

#include "WireCellClus/Facade_Grouping.h"
#include "WireCellClus/Facade_Cluster.h"
#include "WireCellClus/TrackFitting.h"

using namespace WireCell;
using namespace WireCell::Clus;

// TC-1: postcondition from default (null) state.
TEST_CASE("TrackFitting: clear_segments leaves m_grouping null and geometry maps empty")
{
    TrackFitting tf;
    REQUIRE(tf.grouping() == nullptr);
    tf.clear_segments();
    CHECK(tf.grouping() == nullptr);
    CHECK(tf.get_wpid_offsets().empty());
    CHECK(tf.get_wpid_slopes().empty());
}

// TC-2: idempotency from default state.
TEST_CASE("TrackFitting: repeated clear_segments maintains null m_grouping invariant")
{
    TrackFitting tf;
    for (int i = 0; i < 5; ++i) {
        tf.clear_segments();
        CHECK(tf.grouping() == nullptr);
        CHECK(tf.get_wpid_offsets().empty());
        CHECK(tf.get_wpid_slopes().empty());
    }
}

// TC-3: TRUE REGRESSION — clear_segments() must reset m_grouping that was
// made non-null by preload_clusters().
//
// Implementation insight (empirically confirmed, PBS job 7448692):
//   preload_clusters() sets m_grouping = cluster->grouping() (assignment) BEFORE
//   calling BuildGeometry().  With a bare Grouping (no anodes), BuildGeometry()
//   → fill_cache() immediately throws WireCell::ValueError("anode is null")
//   because Grouping::fill_cache() checks m_anodes.size()==0 before computing wpids.
//   The exception propagates, but m_grouping is already non-null.
//
//   REQUIRE_THROWS documents this expected exception; execution then continues
//   with m_grouping non-null, and clear_segments() is the regression target.
//
// Expected results:
//   pre-fix  0869668: CHECK(grouping()==nullptr) FAILS (m_grouping retained)
//   post-fix 983495f: CHECK(grouping()==nullptr) PASSES (m_grouping = nullptr added)
TEST_CASE("TrackFitting: clear_segments resets m_grouping set by preload_clusters")
{
    using namespace WireCell::PointCloud::Tree;
    using namespace WireCell::Clus::Facade;

    Points::node_t root;
    Grouping* g = root.value.facade<Grouping>();
    Cluster& cl = g->make_child();

    TrackFitting tf;
    REQUIRE(tf.grouping() == nullptr);

    // preload_clusters() line order (both 0869668 and 983495f):
    //   m_grouping = cluster->grouping();  // sets m_grouping non-null
    //   BuildGeometry();                   // throws "anode is null" → exception propagates
    // REQUIRE_THROWS aborts TC-3 if no exception is thrown (future-proofs against
    // no-anode becoming a no-op rather than an error).
    REQUIRE_THROWS(tf.preload_clusters({&cl}));

    // After the exception, m_grouping IS non-null (assignment ran before throw).
    REQUIRE(tf.grouping() != nullptr);

    // Simulate the inter-event reset (mirrors TaggerCheckSTM::operator() path).
    tf.clear_segments();

    // [REGRESSION PIN] m_grouping must be null after clear_segments().
    // Pre-fix (0869668): no m_grouping = nullptr in clear_segments()
    //   → grouping() returns the stale non-null pointer → FAIL.
    // Post-fix (983495f): m_grouping = nullptr added → grouping() == nullptr → PASS.
    CHECK(tf.grouping() == nullptr);
    CHECK(tf.get_wpid_offsets().empty());
    CHECK(tf.get_wpid_slopes().empty());
}
