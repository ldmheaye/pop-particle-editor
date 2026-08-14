#include <WXBase/WXApp.h>
#include "../include/Frames/PopApplication.h"
#include "../include/PopState.h"
#include "../include/PopStyle.h"

int main()
{
	pop::OnInitUserConfig();

	wx::AppCreateInfo createInfo;
	createInfo.captain = pop::ML("app.title");
	createInfo.imguiSetup = pop::InitializePopStyle;

	wx::AppInit(createInfo);

	wx::AppAppendFrame<pop::PopApplication>();

	wx::AppStart();

	wx::AppQuit();

	return EXIT_SUCCESS;
}
