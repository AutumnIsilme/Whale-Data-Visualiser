#pragma once

class Renderer
{
public:
	Renderer(); // Must do nothing. There is only one of these.
	~Renderer();

	int Init(int width, int height);
	void SetClearColour(float red, float green, float blue, float alpha);
	void Clear();

private:

};