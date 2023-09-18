#include "WireCellUtil/PointTree.h"
#include <boost/container_hash/hash.hpp>

#include <algorithm>
#include <vector>
#include <string>

using namespace WireCell::PointCloud;

std::size_t Tree::Scope::hash() const
{
    std::size_t h = 0;
    boost::hash_combine(h, pcname);
    boost::hash_combine(h, depth);
    for (const auto& c : coords) {
        boost::hash_combine(h, c);
    }
    return h;
}

bool Tree::Scope::operator==(const Tree::Scope& other) const
{
    if (depth != other.depth) return false;
    if (pcname != other.pcname) return false;
    if (coords.size() != other.coords.size()) return false;
    for (size_t ind = 0; ind<coords.size(); ++ind) {
        if (coords[ind] != other.coords[ind]) return false;
    }
    return true;
}
bool Tree::Scope::operator!=(const Tree::Scope& other) const
{
    if (*this == other) return false;
    return true;
}


//
//  Points
//

void Tree::Points::on_construct(Tree::Points::node_t* node)
{
    m_node = node;
}



static
bool in_scope(const Tree::Scope& scope, const Tree::Points::node_t* node, size_t depth)
{
    if (scope.depth > 0 and depth > scope.depth) { // fixme: check innequality
        return false;
    }

    auto& lpcs = node->value.local_pcs();

    auto pcit = lpcs.find(scope.pcname);
    if (pcit == lpcs.end()) {
        return false;
    }
    const auto& ds = pcit->second;
    for (const auto& name : scope.coords) {
        if (!ds.has(name)) {
            return false;
        }
    }
    return true;    
}

bool Tree::Points::on_insert(const Tree::Points::node_path_t& path)
{
    auto* node = path.back();
    size_t depth = path.size();

    for (auto [scope,dds] : m_dds) {
        if (! in_scope(scope, node, depth)) {
            continue;
        }
        
        Dataset& ds = node->value.m_lpcs[scope.pcname];

        size_t beg = dds.npoints();
        dds.append(ds);
        size_t end = dds.npoints();

        auto kdit = m_nfkds.find(scope);
        if (kdit == m_nfkds.end()) {
            continue;
        }
        kdit->second->addpoints(beg, end);
    }

    return true;
}

bool Tree::Points::on_remove(const Tree::Points::node_path_t& path)
{
    raise<ValueError>("Tree::Points::on_remove not implemented");
    // need a way to map from an in-scope ds to a ds in a disjointdataset
    return false;
}
    

const DisjointDataset& Tree::Points::scoped_pc(const Tree::Scope& scope) const
{
    auto it = m_dds.find(scope);
    if (it != m_dds.end()) {
        return it->second;
    }

    // construct and store
    DisjointDataset& dds = m_dds[scope];
    for (auto& nv : m_node->depth(scope.depth)) {
        // local pc dataset
        auto it = nv.m_lpcs.find(scope.pcname);
        if (it == nv.m_lpcs.end()) {
            continue;
        }
        Dataset& ds = it->second;
        // check that it has the coordinate arrays
        std::vector<std::string> have = ds.keys(), want(scope.coords), both;
        std::sort(want.begin(), want.end());
        std::set_intersection(have.begin(), have.end(), want.begin(), want.end(),
                              std::back_inserter(both));

        if (both.size() == want.size()) {
            dds.append(ds);
            continue;
        }
        raise<IndexError>("Tree::Points::scoped_pc %s lacks required coordinate arrays", scope.pcname);
        
    };
    return dds;
}

