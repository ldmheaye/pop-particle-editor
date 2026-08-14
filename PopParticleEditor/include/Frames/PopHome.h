#ifndef __POP_HOME_H__
#define __POP_HOME_H__

#include <WXBase/WXImGuiFrame.h>
#include "../PopEvent.h"

namespace pop
{
	class PopHome final : public wx::ImGuiFrame
	{
	public:
		PopHome();
		virtual ~PopHome();

	public:
		virtual void OnRender() override;

	public:
		Event<>& GetOnCreateNewEvent();
		Event<>& GetOnOpenProjectEvent();

	private:
		Event<> _on_create_new;
		Event<> _on_open_project;
	};
}

#endif // !__POP_HOME_H__
