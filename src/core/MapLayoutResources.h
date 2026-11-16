#pragma once
// Must succeed before loading or mutating a game state on external-map builds.
// Data remains resident until shutdown; subsequent calls never perform IO.
bool beginMapLayoutResources();
