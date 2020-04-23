#include "WireCellAux/SimpleTensor.h"
#include "WireCellUtil/Testing.h"

#include <boost/multi_array.hpp>
#include <iostream>

using namespace WireCell;

int main()
{
    const std::vector<size_t> shape = {3, 4, 5};
    size_t nele = 1;
    for (auto s : shape) {
        nele *= s;
    }
    Aux::SimpleTensor<float> st(shape);
    Assert(st.size() == sizeof(float) * nele);
    auto &d = st.store();
    Assert(d.size() == st.size());

    boost::multi_array_ref<float, 3> mar((float *) d.data(), shape);
    Assert(mar.num_elements() == nele);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 5; ++k) {
                mar[i][j][k] = (i + 1) * 100 + (j + 1) * 10 + k + 1;
            }
        }
    }

    for (size_t ind = 0; ind < mar.num_elements(); ++ind) {
        std::cout << ind << " " << mar.data()[ind] << std::endl;
    }

    return 0;
}
