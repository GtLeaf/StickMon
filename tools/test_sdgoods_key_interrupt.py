#!/usr/bin/env python3
"""Run the native key IRQ setup against the ESP-IDF shared interrupt/wake mode."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    source = (ROOT / 'firmware/sdgoods/main/SdgoodsPlatform.cpp').read_text()
    start = source.index('void SdgoodsPlatform::beginKeyInterrupt(){')
    end = source.index('void SdgoodsPlatform::waitStandby(', start)
    # Compile the production method; only GPIO/task hardware operations are fake.
    code = r'''
#include <cassert>
#include <cstdio>
using TaskHandle_t=void*;
using BaseType_t=int;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=1,pdFALSE=0;
constexpr int BOARD_KEY_GPIO=6,BOARD_KEY_ACTIVE_LEVEL=0;
constexpr int GPIO_INTR_ANYEDGE=3,GPIO_INTR_LOW_LEVEL=4;
int mode=0,notifications=0,level=1;
void (*handler)(void*)=nullptr;void* context=nullptr;
#define ESP_ERROR_CHECK(expression) assert((expression)==ESP_OK)
int gpio_install_isr_service(int){return ESP_OK;}
TaskHandle_t xTaskGetCurrentTaskHandle(){return &notifications;}
int gpio_set_intr_type(int pin,int value){assert(pin==6);mode=value;return ESP_OK;}
int gpio_isr_handler_add(int pin,void (*callback)(void*),void* arg){
    assert(pin==6);handler=callback;context=arg;return ESP_OK;
}
// ESP-IDF gpio_wakeup_enable writes the same interrupt type register.
[[maybe_unused]] int gpio_wakeup_enable(int pin,int value){return gpio_set_intr_type(pin,value);}
[[maybe_unused]] int esp_sleep_enable_gpio_wakeup(){return ESP_OK;}
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t*){assert(task==&notifications);++notifications;}
void portYIELD_FROM_ISR(){}
namespace Platform {void errorLine(const char*){assert(false);}}
namespace Sdgoods {
class SdgoodsPlatform {public: TaskHandle_t inputTask_=nullptr;void beginKeyInterrupt();};
''' + source[start:end] + r'''
}
void drive(int next){
    const bool fire=mode==GPIO_INTR_ANYEDGE?next!=level:mode==GPIO_INTR_LOW_LEVEL&&next==0;
    level=next;if(fire)handler(context);
}
int main(){
    Sdgoods::SdgoodsPlatform board;board.beginKeyInterrupt();
    assert(handler&&board.inputTask_==&notifications);
    drive(0);assert(notifications==1);
    for(int i=0;i<100;++i)drive(0);
    assert(notifications==1); // A held key must not continuously retrigger its ISR.
    drive(1);assert(notifications==2); // Release must still wake the debounce task.
    std::puts("PASS production key IRQ: one press edge, quiet held level, one release edge");
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        (path / 'irq.cpp').write_text(code)
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        str(path / 'irq.cpp'), '-o', str(path / 'irq')], check=True)
        subprocess.run([str(path / 'irq')], check=True)

if __name__ == '__main__':
    main()
