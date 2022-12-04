#pragma once

class InputDevice {
public:
	virtual int GetInputs() = 0;
	virtual ~InputDevice() = default;
};