/** A facade over a PC tree giving semantics to otherwise nodes.
 *
 */

#ifndef WIRECELLIMG_POINTCLOUDFACADE
#define WIRECELLIMG_POINTCLOUDFACADE

#include "WireCellUtil/PointCloudDataset.h"
#include "WireCellUtil/PointTree.h"
#include "WireCellUtil/Point.h"
#include "WireCellUtil/Units.h"

using namespace WireCell;

namespace WireCell::PointCloud::Facade {
    using node_t = WireCell::PointCloud::Tree::Points::node_t;
    using node_ptr = std::unique_ptr<node_t>;
    using geo_point_t = WireCell::Point;
    using geo_vector_t = WireCell::Vector;
    using float_t = double;
    using int_t = int;

    struct TPCParams {
        float_t pitch_u {3*units::mm};
        float_t pitch_v {3*units::mm};
        float_t pitch_w {3*units::mm};
        float_t angle_u {1.0472};  // 60 degrees    uboone geometry ...
        float_t angle_v {-1.0472};  //-60 degrees   uboone geometry ...
        float_t angle_w {0};        // 0 degrees    uboone geometry ...
        float_t tick_width {0.5*1.101*units::mm}; // width corresponding to one tick time
    };

    // Provide common types for an object to be shared via pointer.
    template<typename T>
    struct Shared {

        // The wrapped sub class type.
        using shared_type = T;

        // For holding a facade by a shared pointer.
        using pointer = std::shared_ptr<T>;
        using const_pointer = std::shared_ptr<const T>;

        // Simple collection of shared facades.
        using vector = std::vector<pointer>;
        using const_vector = std::vector<const_pointer>;
    };
    

    class Blob : public Shared<Blob> {
       public:
        Blob(node_t* n);

        geo_point_t center_pos() const;
	int_t num_points() const;
        bool overlap_fast(const Blob& b, const int offset) const;

        /// FIXME: cache all scalers?
        float_t charge {0};
        float_t center_x {0};
        float_t center_y {0};
        float_t center_z {0};
	int_t npoints {0};
	
        int_t slice_index_min {0}; // unit: tick
        int_t slice_index_max {0};

        int_t u_wire_index_min {0};
        int_t u_wire_index_max {0};
        int_t v_wire_index_min {0};
        int_t v_wire_index_max {0};
        int_t w_wire_index_min {0};
        int_t w_wire_index_max {0};

        node_t* node() { return m_node; }
        const node_t* node() const { return m_node; }

      private:
        node_t* m_node;  /// do not own
    };

    // A cluster facade adds to a PC tree node semantics of a set of blobs
    // likely due to connected activity.
    class Cluster : public Shared<Cluster> {
        Blob::vector m_blobs;
        node_t* m_node;  /// do not own
    public:
        Cluster(node_t* n);

        node_t* node() { return m_node; }
        const node_t* node() const { return m_node; }

        // Access the collection of blobs.
        Blob::const_vector blobs() const {
            Blob::const_vector ret(m_blobs.size());
            std::transform(m_blobs.begin(), m_blobs.end(), ret.begin(),
                           [](auto& bptr) { return std::const_pointer_cast<const Blob>(bptr); });
            return ret;
        }
        Blob::vector blobs() { return m_blobs; }

        geo_point_t calc_ave_pos(const geo_point_t& origin, const double dis, const int alg = 0) const;
	std::pair<geo_point_t, Blob::const_pointer > get_closest_point_mcell(const geo_point_t& origin) const;
	std::map<Blob::const_pointer, geo_point_t> get_closest_mcell(const geo_point_t& p, double search_radius) const;
	std::pair<geo_point_t, double> get_closest_point_along_vec(geo_point_t& p_test, geo_point_t dir, double test_dis, double dis_step, double angle_cut, double dis_cut) const;
	int get_num_points(const geo_point_t& point,   double dis) const;
	int get_num_points() const;
	std::pair<int, int> get_num_points(const geo_point_t& point, const geo_point_t& dir) const;
	std::pair<int, int> get_num_points(const geo_point_t& point, const geo_point_t& dir, double dis) const;

	std::pair<geo_point_t, geo_point_t> get_earliest_latest_points() const;
	std::pair<geo_point_t, geo_point_t> get_highest_lowest_points() const;
	
	
        Blob::const_vector is_connected(const Cluster& c, const int offset) const;
        // alg 0: cos(theta), 1: theta
        std::pair<double, double> hough_transform(const geo_point_t& origin, const double dis, const int alg = 1) const;
        geo_point_t vhough_transform(const geo_point_t& origin, const double dis, const int alg = 1) const;

        // get the number of unique uvwt bins
        std::tuple<int, int, int, int> get_uvwt_range() const;
        double get_length(const TPCParams& tp) const;

	// added 
	Blob::const_pointer get_first_blob() const;
	Blob::const_pointer get_last_blob() const;
	
       private:
	// needed a sorted map ...
        //std::unordered_multimap<int, Blob::const_pointer> m_time_blob_map;
	std::multimap<int, Blob::const_pointer> m_time_blob_map;
    };


    inline double cal_proj_angle_diff(const geo_vector_t& dir1, const geo_vector_t& dir2, double plane_angle) {
        geo_vector_t temp_dir1;
        geo_vector_t temp_dir2;

        temp_dir1.set(dir1.x(), 0, -sin(plane_angle) * dir1.y() + cos(plane_angle) * dir1.z());
        temp_dir2.set(dir2.x(), 0, -sin(plane_angle) * dir2.y() + cos(plane_angle) * dir2.z());

        return temp_dir1.angle(temp_dir2);
    }

    inline bool is_angle_consistent(const geo_vector_t& dir1, const geo_vector_t& dir2, bool same_direction, double angle_cut, double uplane_angle, double vplane_angle, double wplane_angle, int num_cut=2) {
        double angle_u = cal_proj_angle_diff(dir1, dir2, uplane_angle);
        double angle_v = cal_proj_angle_diff(dir1, dir2, vplane_angle);
        double angle_w = cal_proj_angle_diff(dir1, dir2, wplane_angle);
        int num = 0;
        // input is degrees ...
        angle_cut *= 3.1415926 / 180.;

        if (same_direction) {
            if (angle_u <= angle_cut) num++;
            if (angle_v <= angle_cut) num++;
            if (angle_w <= angle_cut) num++;
        } else {
            if ((3.1415926 - angle_u) <= angle_cut) num++;
            if ((3.1415926 - angle_v) <= angle_cut) num++;
            if ((3.1415926 - angle_w) <= angle_cut) num++;
        }

        if (num >= num_cut) return true;
        return false;
    }

}  // namespace WireCell::PointCloud

#endif
