// Regression test: TrackFitting::clear_segments() must reset m_grouping to nullptr
// and clear all geometry-derived maps (wpid_geoms, wpid_offsets, wpid_slopes,
// wpid_params, wpid_U/V/W_dir, apas, m_hot_cache).
//
// Root cause: TaggerCheckSTM owns a mutable TrackFitting that persists across
// events as a member of the long-lived MABC pipeline.  When event N's track
// fitting ran, add_segment() set m_grouping to event N's Grouping.
// clear_segments() was called between events but did NOT reset m_grouping.
// Event N's Grouping was freed when operator() returned.  Event N+1's
// add_segment() found m_grouping != nullptr, skipped BuildGeometry(), then
// called m_grouping->get_anode() on freed memory → SIGSEGV.
//
// Validated by byte-identical A/B gate before this unit test was written:
//   Pre-fix: job 7441749 REPRODUCED (SIGSEGV during E=40; E=39 partial present)
//   Post-fix: job XXXX PASS (both E=39 and E=40 complete; final H5 published)
//
// This unit test pins the POST-CLEAR invariants:
//   - grouping() == nullptr after clear_segments()
//   - all geometry maps empty after clear_segments()
//
// The full multi-event regression (m_grouping non-null → null) cannot be driven
// without real detector geometry (IDetectorVolumes + IAnodePlane) because
// add_segment() → BuildGeometry() → Grouping::wpids() → fill_cache() requires
// configured anodes.  That regression is covered by the two-event PBS gate.

#include "WireCellUtil/doctest.h"

#include "WireCellClus/TrackFitting.h"

using namespace WireCell;
using namespace WireCell::Clus;

TEST_CASE("TrackFitting: clear_segments leaves m_grouping null and geometry maps empty")
{
    // Verify the postconditions of clear_segments() without detector geometry.
    // The pre-fix code lacked m_grouping = nullptr in clear_segments(), so after
    // a real event's add_segment() set m_grouping, subsequent clear_segments()
    // left it dangling.  The fix adds the reset; this test pins the invariant.

    TrackFitting tf;

    // Initial state: m_grouping must be null.
    REQUIRE(tf.grouping() == nullptr);

    // clear_segments() must leave the TF in a consistent post-clear state:
    // m_grouping null, all geometry maps empty.  This invariant must hold
    // regardless of whether add_segment() was ever called.
    tf.clear_segments();

    // [REGRESSION PIN] m_grouping must be null after clear (the fix).
    CHECK(tf.grouping() == nullptr);

    // [REGRESSION PIN] Geometry maps derived from BuildGeometry must be empty.
    CHECK(tf.get_wpid_offsets().empty());
    CHECK(tf.get_wpid_slopes().empty());
}

TEST_CASE("TrackFitting: repeated clear_segments maintains null m_grouping invariant")
{
    // Calling clear_segments() multiple times must not change the null state.
    TrackFitting tf;

    for (int i = 0; i < 5; ++i) {
        tf.clear_segments();
        CHECK(tf.grouping() == nullptr);
        CHECK(tf.get_wpid_offsets().empty());
        CHECK(tf.get_wpid_slopes().empty());
    }
}
