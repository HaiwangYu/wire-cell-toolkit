#include <memory>
#include "WireCellUtil/IComponent.h"

class IFunctor : public WireCell::IComponent<IFunctor>
{
   public:
    virtual ~IFunctor() {}

    virtual std::string signature() = 0;
};

template <typename InputType, typename OutputType>
class IFunctorT : public IFunctor
{
   public:
    typedef InputType input_type;
    typedef OutputType output_type;
    typedef IFunctorT<InputType, OutputType> this_type;

    virtual ~IFunctorT() {}

    virtual std::string signature() { return typeid(this_type).name(); }

    virtual bool operator()(const std::shared_ptr<const input_type> &in,
                            std::shared_ptr<const output_type> &out) = 0;
};

class IMyIFConverter : public IFunctorT<int, float>
{
   public:
    virtual ~IMyIFConverter() {}
};

class MyIFConverter : public IMyIFConverter
{
   public:
    virtual ~MyIFConverter() {}

    virtual bool operator()(const std::shared_ptr<const input_type> &in,
                            std::shared_ptr<const output_type> &out)
    {
        out = std::make_shared<const float>(*in);
        return true;
    }
};

class INode : public WireCell::IComponent<INode>
{
   public:
    virtual ~INode() {}

    virtual std::string functor_type_name() = 0;
};

#include "WireCellUtil/Testing.h"

int main()
{
    // emulate NF lookup
    std::shared_ptr<IFunctor> fun(new MyIFConverter);

    // emulate node maker and wrapper
    auto myfun = std::dynamic_pointer_cast<IMyIFConverter>(fun);

    // emulate running a node
    auto in = std::make_shared<const int>(42);
    auto out = std::make_shared<const float>(0);
    bool ok = (*myfun)(in, out);

    Assert(ok);
    Assert(42.0 == *out);
    return 0;
}
