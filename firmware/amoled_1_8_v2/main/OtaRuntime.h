#pragma once
#include <cstdint>
class Canvas565;
namespace AmoledV2::Ota {
// Maintenance runs before AmoledApp/Claw, with exclusive ownership of Wi-Fi.
void armBootGuard();
bool pendingVerification();
bool bootResourcesHealthy();
bool confirmBoot();
bool requestMaintenance();
bool consumeMaintenanceRequest();
bool beginMaintenance();
void render(Canvas565& canvas);
bool consumeRedraw();
void exitMaintenance();
}
