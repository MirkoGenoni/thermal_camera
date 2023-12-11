#pragma once
#include <drivers/mlx90640.h>
#include "memoryState.h"
#include <memory>

void saveImage(MemoryState *state, std::unique_ptr<MLX90640MemoryFrame> memoryFrame, int imageSize);