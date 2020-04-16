/*!
 * FlowConfigurable provides common configuration for any component
 * that is a sink/extractor or source/injector of WCT / ZIO flow data.
 *
 * It provides a ZIO flow node with a single port.
 *
 * It provides configurable management of the sending of the BOT to
 * assist in ZPB.
 *
 */

#ifndef WIRECELLZPB_FLOWCONFIGURABLE
#define WIRECELLZPB_FLOWCONFIGURABLE

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/ITerminal.h"
#include "WireCellUtil/Logging.h"
#include "zio/flow.hpp"
#include "zio/node.hpp"

#include "WireCellIface/ITensorSet.h"

namespace WireCell
{
    namespace Zio
    {
        class FlowConfigurable : public WireCell::IConfigurable,
                                 public WireCell::ITerminal
        {
           public:
            FlowConfigurable(const std::string &direction,
                             const std::string &nodename = "");
            virtual ~FlowConfigurable();

            virtual WireCell::Configuration default_configuration() const;
            virtual void configure(const WireCell::Configuration &config);

            virtual void finalize();

           protected:
            int m_timeout{1000}, m_credit{10};
            std::string m_portname{"flow"}, m_direction{"extract"}, m_bot_label{""};
            zio::Node m_node;
            zio::level::MessageLevel m_level{zio::level::info};
            int m_stype{ZMQ_CLIENT};
            zio::headerset_t m_headers{};

            Log::logptr_t l;

            /// Subclass should set this to the type of protobuf payload message
            std::string m_type_name{""};

            /// Subclass may use this for all but BOT.
            typedef std::unique_ptr<zio::flow::Flow> flowptr_t;
            flowptr_t m_flow;

            /// Subclass must call this before any actual flow.  It is
            /// safe to call at the top of each execution.  If it
            /// returns false, no flow is possible.  The m_flow will
            /// be invalid.
            bool pre_flow();

            /// Give subclass a chance to add to a configuration
            virtual void
            user_default_configuration(WireCell::Configuration &cfg) const {};

            /// Give subclass a chance to read a configuration.  Ports
            /// are not yet online.
            virtual bool user_configure(const WireCell::Configuration &cfg)
            {
                return true;
            };

            /// Called after going online and before configuration phase is over
            virtual bool user_online() { return true; }

           public:
            /// Pack the ITensorSet into a ZIO Message
            static zio::Message pack(const ITensorSet::pointer &itens);

            /// Unpack ZIO Message to ITensorSet
            static ITensorSet::pointer unpack(const zio::Message &zmsg);

           private:
            // assure pre_flow() body called just once.
            bool m_did_bot{false};
        };
    }  // namespace Zio
}  // namespace WireCell

#endif
