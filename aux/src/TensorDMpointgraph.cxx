#include "WireCellAux/TensorDMpointgraph.h"
#include "WireCellAux/TensorDMdataset.h"
#include "WireCellAux/TensorDMcommon.h"
#include "WireCellAux/SimpleTensor.h"


using namespace WireCell;
using namespace WireCell::Aux;
using namespace WireCell::Aux::TensorDM;


WireCell::PointGraph
WireCell::Aux::TensorDM::as_pointgraph(const ITensor::vector& tens,
                                       const std::string& datapath)
{
    TensorIndex ti(tens);
    return as_pointgraph(ti, datapath);
}
WireCell::PointGraph
WireCell::Aux::TensorDM::as_pointgraph(const TensorIndex& ti,
                                       const std::string& datapath)
{
    auto top = ti.at(datapath, "pcgraph");
    const auto& topmd = top->metadata();
    auto nodes = as_dataset(ti, topmd["nodes"].asString());
    auto edges = as_dataset(ti, topmd["edges"].asString());
    return PointGraph(nodes, edges);
}

ITensor::vector
WireCell::Aux::TensorDM::as_tensors(const PointGraph& pcgraph,
                                    const std::string& datapath)
{
    Configuration md;
    md["datatype"] = "pcgraph";
    md["datapath"] = datapath;
    md["nodes"] = datapath + "/nodes";
    md["edges"] = datapath + "/edges";

    ITensor::vector tens;
    tens.push_back(std::make_shared<SimpleTensor>(md));

    auto nodes = as_tensors(pcgraph.nodes(), datapath + "/nodes");
    tens.insert(tens.end(), nodes.begin(), nodes.end());

    auto edges = as_tensors(pcgraph.edges(), datapath + "/edges");
    tens.insert(tens.end(), edges.begin(), edges.end());

    return tens;
}

