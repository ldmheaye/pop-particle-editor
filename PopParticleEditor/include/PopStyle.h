#ifndef POP_STYLE_H
#define POP_STYLE_H

namespace pop
{
	void InitializePopStyle(float dpiScale = 1.0f);
	void ApplyPopStyle(float dpiScale = 1.0f);
	bool LoadPopFonts();

	void PushPrimaryButtonStyle();
	void PopPrimaryButtonStyle();
}

#endif // POP_STYLE_H
