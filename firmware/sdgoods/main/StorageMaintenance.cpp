// SPDX-License-Identifier: Apache-2.0
#include "StorageMaintenance.h"
#include "ResourceBanks.h"
#include "SdgoodsPlatform.h"
#include "RoomSession.h"
#include "presentation/PixelRenderer.h"
#include "RecoveryText.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_ota_ops.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace Sdgoods {
namespace {
bool consoleReady=false;
bool discardCommand=false;
std::string command;
void runUpdate(SdgoodsPlatform& board,RoomSession& session,bool ready,const char* json) {
    // A shared LittleFS image cannot be erased while the running application
    // is using it.  Resource and application changes therefore use the full
    // ROM installer until an atomic staging format is introduced.
    (void)board; (void)session; (void)ready; (void)json;
    std::puts("SMON ERROR shared-resource-update-disabled");
}
}
void beginMaintenanceConsole() {
    if(!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config{};config.rx_buffer_size=8192;config.tx_buffer_size=1024;
        consoleReady=usb_serial_jtag_driver_install(&config)==ESP_OK;
    } else consoleReady=true;
    // stdout must share the installed interrupt driver. The default VFS polls
    // the hardware FIFO directly and can lose an ACK when the TX ISR runs.
    if(consoleReady)usb_serial_jtag_vfs_use_driver();
}
void pollMaintenanceConsole(SdgoodsPlatform& board,RoomSession& session,bool ready) {
    if(!consoleReady)return;
    uint8_t bytes[256];const int n=usb_serial_jtag_read_bytes(bytes,sizeof(bytes),0);
    for(int i=0;i<n;++i) {
        if(bytes[i]=='\n') {
            if(discardCommand) {discardCommand=false;command.clear();continue;}
            if(command=="SMON-HELLO")std::printf("SMON STATUS slot=%d layout=%u ready=%u\n",runningSlot(),storageLayoutHealthy(),ready);
            else if(command.rfind("SMON-UPDATE ",0)==0)runUpdate(board,session,ready,command.c_str()+12);
            command.clear();std::fflush(stdout);
        } else if(bytes[i]!='\r') {
            if(discardCommand)continue;
            if(command.size()<8192)command+=char(bytes[i]);else {command.clear();discardCommand=true;}
        }
    }
}
void storageRecovery(SdgoodsPlatform& board,const char* reason) {
    PixelRenderer::bind(board.frameBuffer());PixelRenderer::clear(0);
    auto pixel=[](int x,int y,uint16_t color){PixelRenderer::canvas().drawPixel(x,y,color);};
    RecoveryText::draw(84,106,"STICKMON STORAGE",0xffff,pixel);
    const size_t length=std::strlen(reason);
    RecoveryText::draw(int(180-length*3),150,reason,0xffe0,pixel,1);
    RecoveryText::draw(42,184,"USE USB INSTALLER",0xffff,pixel);
    RecoveryText::draw(72,222,"SAVE DATA RETAINED",0xffff,pixel,1);
    board.present();
}
bool confirmStorageBoot() {
    const auto* p=esp_ota_get_running_partition();esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(p,&state)!=ESP_OK)return false;
    if(state==ESP_OTA_IMG_PENDING_VERIFY)return esp_ota_mark_app_valid_cancel_rollback()==ESP_OK;
    return state==ESP_OTA_IMG_VALID;
}
} // namespace Sdgoods
