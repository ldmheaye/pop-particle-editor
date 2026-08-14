#ifndef __POP_APPLICATION_H__
#define __POP_APPLICATION_H__

#include <WXBase/WXImGuiFrame.h>
#include "PopHome.h"
#include "PopWorkBench.h"

#include <string>

namespace pop
{
	class PopApplication final : public wx::ImGuiFrame
	{
	private:
		enum class State
		{
			STATE_HOME,
			STATE_BENCH
		};

	public:
		PopApplication();
		virtual ~PopApplication();

	public:
		virtual void OnRender() override;

	private:
		void _create_new();
		void _open_project();
		void _save_project(bool saveAs);
		void _publish_project();
		void _show_notification(std::string message, bool error);
		void _render_notification();

	private:
		PopHome _home;
		PopWorkBench _bench;
		State _state;
		std::string _notification_message;
		wx::Float64 _notification_until = 0.0;
		bool _notification_error = false;
	};
}

#endif // !__POP_APPLICATION_H__
