
#include "ResponseRouter.h"
#include "kademlia/log.hpp"

using Poco::Net::SocketProactor;


namespace kademlia {
namespace detail {


	ResponseRouter::ResponseRouter(SocketProactor &io_service): timer_(io_service)
	{
	}


	void ResponseRouter::handle_new_response(endpoint_type const &sender, Header const &h,
											 buffer::const_iterator i, buffer::const_iterator e)
	{
		LOG_DEBUG(ResponseRouter, this) << "dispatching response from " << sender.address_.toString() << ':' << sender.port_ << std::endl;
		// Try to forward the message to its associated callback.
		auto failure = response_callbacks_.dispatch_response(sender, h, i, e);
		if (failure == UNASSOCIATED_MESSAGE_ID)// Unknown or unassociated responses discarded.
			LOG_DEBUG(ResponseRouter, this) << "dropping unknown response." << std::endl;
	}

} }
