// SPDX-License-Identifier: Apache-2.0
#pragma once
namespace Sdgoods {
class SdgoodsPlatform;
class RoomSession;
void beginMaintenanceConsole();
void pollMaintenanceConsole(SdgoodsPlatform&,RoomSession&,bool gameReady);
void storageRecovery(SdgoodsPlatform&,const char* reason);
bool confirmStorageBoot();
} // namespace Sdgoods
