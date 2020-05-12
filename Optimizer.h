#pragma once

#include "Common.h"
#include "IntermediateCode.h"

class Optimizer {
private:
	IntermediateCode code;
public:
	Optimizer(IntermediateCode& code);

};