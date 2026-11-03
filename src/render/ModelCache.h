#pragma once

#include <string>
#include <vector>

#include <raylib.h>

// Lazy cache of GLB models from the model directory, keyed by file name (no extension).
// Needs an open window (loading uploads to the GPU).
//
// Every converted Synty GLB embeds its own copy of the same 4096x4096 atlas; loaded as-is,
// each model would cost ~64 MB of VRAM. The first atlas that is loaded is shared by every
// later model with the same texture size, and the duplicate is unloaded right away.

// Returns nullptr when the file is missing or fails to load (callers fall back to a box)
const Model* GetModel(const std::string& name);

// Names of every GLB available in the model directory, sorted
const std::vector<std::string>& ListModelNames();

void UnloadAllModels();
