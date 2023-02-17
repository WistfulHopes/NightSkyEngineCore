﻿#include "InputBuffer.h"
#include "Bitflags.h"

void InputBuffer::Tick(int32_t Input)
{
	for (int32_t i = 0; i < 89; i++)
	{
		InputBufferInternal[i] = InputBufferInternal[i + 1];
		InputDisabled[i] = InputDisabled[i + 1];
	}
	InputBufferInternal[89] = Input;
	InputDisabled[89] = 0;
}

bool InputBuffer::CheckInputCondition(const InputCondition InputCondition)
{
	for (int i = 0; i < 20; i++)
	{
		if (i >= InputCondition.Sequence.size())
		{
			InputSequence[i] = -1;
			continue;
		}
		InputSequence[i] = InputCondition.Sequence[i].InputFlag;
	}
	Lenience = InputCondition.Lenience;
	ImpreciseInputCount = InputCondition.ImpreciseInputCount;
	bInputAllowDisable = InputCondition.bInputAllowDisable;
	switch (InputCondition.Method)
	{
	case InputMethod::Normal:
		return CheckInputSequence();
	case InputMethod::Strict:
		return CheckInputSequenceStrict();
	case InputMethod::Once:
		return CheckInputSequenceOnce();
	case InputMethod::OnceStrict:
		return CheckInputSequenceOnceStrict();
	default:
		return false;
	}
}

bool InputBuffer::CheckInputSequence()
{
	int32_t InputIndex = -10;
	for (int32_t i = 19; i > -1; i--)
	{
		if (InputSequence[i] != -1)
		{
			InputIndex = i;
			break;
		}
	}
	int32_t FramesSinceLastMatch = 0; //how long it's been since last input match
	bool NoMatches = true;

	for (int32_t i = 89; i >= 0; i--)
	{
		if (InputIndex == -1) //check if input sequence has been fully read
			return true;
		
		if (NoMatches && InputDisabled[i] == InputBufferInternal[i] && bInputAllowDisable)
			return false;
		
		const int32_t NeededInput = InputSequence[InputIndex];
		if (FramesSinceLastMatch > Lenience)
			return false;
		FramesSinceLastMatch++;

		if ((InputBufferInternal[i] & NeededInput) == NeededInput) //if input matches...
		{
			NoMatches = false;
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
		}
	}

	return false;
}

bool InputBuffer::CheckInputSequenceStrict()
{
	int32_t InputIndex = -10;
	for (int32_t i = 19; i > -1; i--)
	{
		if (InputSequence[i] != -1)
		{
			InputIndex = i;
			break;
		}
	}
	int32_t FramesSinceLastMatch = 0; //how long it's been since last input match
	int32_t ImpreciseMatches = 0;
	bool NoMatches = true;
	
	for (int32_t i = 89; i >= 0; i--)
	{
		if (InputIndex == -1) //check if input sequence has been fully read
			return true;

		if (NoMatches && InputDisabled[i] == InputBufferInternal[i] && bInputAllowDisable)
			return false;
		
		const int32_t NeededInput = InputSequence[InputIndex];
		if (FramesSinceLastMatch > Lenience)
			return false;
		FramesSinceLastMatch++;

		if ((InputBufferInternal[i] ^ NeededInput) << 27 == 0) //if input matches...
		{
			NoMatches = false;
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
			continue;
		}
		if ((InputBufferInternal[i] & NeededInput) == NeededInput) //if input doesn't match precisely...
		{
			NoMatches = false;
			if (ImpreciseMatches >= ImpreciseInputCount)
				continue;
			ImpreciseMatches++;
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
		}
	}

	return false;
}

bool InputBuffer::CheckInputSequenceOnce()
{
	int32_t InputIndex = -10;
	for (int32_t i = 19; i > -1; i--)
	{
		if (InputSequence[i] != -1)
		{
			InputIndex = i;
			break;
		}
	}
	int32_t FramesSinceLastMatch = 0; //how long it's been since last input match

	for (int32_t i = 89; i >= 0; i--)
	{
		if (InputDisabled[i] == InputBufferInternal[i] && bInputAllowDisable)
			return false;

		if (InputIndex < 0) //check if input sequence has been fully read
		{
			if (InputIndex <= -Lenience)
				return false;
			if (!(InputBufferInternal[i] & InputSequence[0]))
				return true;
			InputIndex--;
			continue;
		}
		const int32_t NeededInput = InputSequence[InputIndex];

		if (FramesSinceLastMatch > Lenience)
			return false;
		FramesSinceLastMatch++;

		if ((InputBufferInternal[i] & NeededInput) == NeededInput) //if input matches...
		{
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
		}
	}

	return false;
}

bool InputBuffer::CheckInputSequenceOnceStrict()
{
	int32_t InputIndex = -10;
	for (int32_t i = 19; i > -1; i--)
	{
		if (InputSequence[i] != -1)
		{
			InputIndex = i;
			break;
		}
	}
	int32_t FramesSinceLastMatch = 0; //how long it's been since last input match
	int32_t ImpreciseMatches = 0;

	for (int32_t i = 89; i >= 0; i--)
	{
		if (InputDisabled[i] == InputBufferInternal[i] && bInputAllowDisable)
			return false;

		if (InputIndex < 0) //check if input sequence has been fully read
		{
			if (InputIndex <= -Lenience)
				return false;
			if (!(InputBufferInternal[i] & InputSequence[0]))
				return true;
			InputIndex--;
			continue;
		}
		const int32_t NeededInput = InputSequence[InputIndex];

		if (FramesSinceLastMatch > Lenience)
			return false;
		FramesSinceLastMatch++;

		if ((InputBufferInternal[i] ^ NeededInput) << 27 == 0) //if input matches...
		{
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
			continue;
		}
		if ((InputBufferInternal[i] & NeededInput) == NeededInput) //if input matches...
		{
			if (ImpreciseMatches >= ImpreciseInputCount)
				continue;
			ImpreciseMatches++;
			InputIndex--; //advance sequence
			FramesSinceLastMatch = 0; //reset last match
			i--;
		}
	}

	return false;
}

void InputBuffer::FlipInputsInBuffer()
{
	for (int i = 0; i < 90; i++)
	{
		const unsigned int Bit1 = (InputBufferInternal[i] >> 2) & 1;
		const unsigned int Bit2 = (InputBufferInternal[i] >> 3) & 1;
		unsigned int x = (Bit1 ^ Bit2);

		x = x << 2 | x << 3;

		InputBufferInternal[i] = InputBufferInternal[i] ^ x;
	}
}
