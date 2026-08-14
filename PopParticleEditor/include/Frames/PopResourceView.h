#ifndef __POP_RESOURCE_VIEW_H__
#define __POP_RESOURCE_VIEW_H__

#include <WXBase/WXDefinitions.h>
#include <string>
#include <list>

#include "../PopResource.h"

namespace pop
{
	class PopResourceView
	{
	public:
		PopResourceView();
		~PopResourceView();

	public:
		void OnRender();

	private:
		wx::Float32 _icon_size;
	};
}

#endif // !__POP_RESOURCE_VIEW_H__
