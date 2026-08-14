#ifndef __POP_EVENT_H__
#define __POP_EVENT_H__

#include <functional>
#include <unordered_map>
#include <WXBase/WXDefinitions.h>

namespace pop
{
	template<typename ...Args>
	class Event
	{
	public:
		using Handler = std::function<void(Args...)>;
		using HandlerID = wx::Size;

	public:
		Event() :_id_counter(0) {};
		~Event() {};

	public:
		HandlerID Subscribe(const Handler& handler)
		{
			_id_counter++;
			_handlers.emplace(_id_counter, handler);
			return _id_counter;
		}

		void Unsubscribe(HandlerID id)
		{
			if (!_handlers.count(id))
				return;
			_handlers.erase(id);
		}

		void Invoke(Args... args)
		{
			for (auto & [_, handler] : _handlers)
			{
				handler(args...);
			}
		}

	private:
		HandlerID _id_counter;
		std::unordered_map<HandlerID, Handler> _handlers;
	};
}

#endif // !__POP_EVENT_H__
