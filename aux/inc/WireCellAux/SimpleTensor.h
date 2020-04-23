#ifndef WIRECELL_AUX_SIMPLETENSOR
#define WIRECELL_AUX_SIMPLETENSOR

#include <boost/multi_array.hpp>
#include "WireCellIface/ITensor.h"

namespace WireCell {
    namespace Aux {
        template <typename ElementType>
        class SimpleTensor : public WireCell::ITensor {
           public:
            typedef ElementType element_t;

            SimpleTensor(const shape_t &shape)
            {
                size_t nbytes = element_size();
                for (const auto &s : shape) {
                    nbytes *= s;
                }
                m_store.resize(nbytes);
                m_shape = shape;
            }
            virtual ~SimpleTensor() {}

            /** Creator may use the underlying data store allocated in
   * contructor in a non-const manner to set the elements.

   Eg, using boost::multi_array_ref:

   SimpleTensor<float> tens({3,4,5});
   auto& d = tens.store();
   boost::multi_array_ref<float, 3> ma(d.data(), {3,4,5});
   md[1][2][3] = 42.0;
  */
            std::vector<std::byte> &store() { return m_store; }

            // ITensor const interface.
            virtual std::type_index element_type() const { return typeid(element_t); }
            virtual size_t element_size() const { return sizeof(element_t); }

            virtual shape_t shape() const { return m_shape; }

            virtual const std::byte *data() const { return m_store.data(); }
            virtual size_t size() const { return m_store.size(); }

           private:
            std::vector<std::byte> m_store;
            std::vector<size_t> m_shape;
        };
    }  // namespace Aux
}  // namespace WireCell

#endif
