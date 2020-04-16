/** Some tools that operate on frame-related interfaces.

    fixme: this should probably go into some WireCellItools package.
 */

#ifndef WIRECELL_FRAMETOOLS
#define WIRECELL_FRAMETOOLS

#include "WireCellIface/IFrame.h"
#include "WireCellUtil/Array.h"

namespace WireCell
{
    namespace FrameTools
    {
        /// Return a vector of traces which have no trace tags.  Here,
        /// any frame tags are ignored.  Returned vector of traces has
        /// undefined order.
        ITrace::vector untagged_traces(IFrame::pointer frame);

        /// Return a vector of traces for the matching tag.  First, if
        /// there is a matching trace tag, all its traces are
        /// returned.  Else, if the frame as a whole is tagged, the
        /// entire frame of traces is returned.  Else, the returned
        /// vector is empty.  If the tag consisting of the empty
        /// string then untagged_traces() is called.  Returned vector
        /// of traces has undefined order.
        ITrace::vector tagged_traces(IFrame::pointer frame, IFrame::tag_t tag);

        /// Return a one-to-one vector of channels from a vector of
        /// traces.
        ///
        /// Note, you probably want to get a sorted/unique version of
        /// this vector for later use.  Do so something like:
        ///
        ///   auto ch = channels(traces);
        ///   std::sort(ch.begin(), ch.end());
        ///   auto end = std::unique(ch.begin(), ch.end());
        ///   ch.resize(std::distance(ch.begin(), end));
        ///
        typedef std::vector<int> channel_list;
        channel_list channels(const ITrace::vector &traces);

        /// Return the tbin range of the traces.  The first value is
        /// minimum of all tbins and the second is maximum of all
        /// tbin+size where size is number of elements in the charge
        /// array.
        std::pair<int, int> tbin_range(const ITrace::vector &traces);

        /// Fill a 2D [nchannels/nrows X nticks/ncolumns] array by
        /// adding the charge information in the given traces.  The
        /// channel_list range is an ordered sequence of channels used
        /// to associate traces to rows in the array.  Ie, any trace
        /// with the same channel number as pointed to ch_begin will
        /// be in row 0 of the array.  The "tbin" gives the time bin
        /// of column 0 of the array and is compared to the tbin value
        /// of the individual traces.  Note, traces which are not
        /// refered to in the channel list or which are outside the
        /// array are ignored and not all channels need to have
        /// associated traces.
        void fill(Array::array_xxf &array, const ITrace::vector &traces,
                  channel_list::iterator ch_begin, channel_list::iterator ch_end,
                  int tbin = 0);

        /// Compare the time span of a frame to a time.
        ///
        /// Return 0 if the frame time span covers the target time.
        /// Return -1 if the frame is wholly before the target time.
        /// Return +1 if the frame is wholly after the target time.
        ///
        /// Note, the frame pointer must be valid and the frame must
        /// have traces.
        ///
        /// Note, if the low-edge of the minimum tick or the high-edge
        /// of the maximum tick is exactly at the target time then the
        /// frame span is not considered to cover the target time.
        int frmtcmp(IFrame::pointer frame, double time);

        /// Split one frame into two.  A new .first frame will contain
        /// traces with all samples taken before the given time and a
        /// new .second frame with traces containing samples all taken
        /// on or after the given time.  If the original frame time
        /// span does not cover the target time then the original
        /// frame is returned in the associated half of the pair and
        /// the other half will hold the nullptr.
        ///
        /// Note, the frame pointer must be valid and the frame must
        /// have traces.
        std::pair<IFrame::pointer, IFrame::pointer> split(IFrame::pointer frame,
                                                          double time);

    }  // namespace FrameTools
}  // namespace WireCell

#endif
