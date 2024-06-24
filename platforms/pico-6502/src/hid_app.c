#include "tusb.h"
#include "class/hid/hid.h"

extern void kbd_raw_key_down(int code);
extern void kbd_raw_key_up(int code);

uint8_t const conv_table[128][2] = {HID_KEYCODE_TO_ASCII};

static inline bool find_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

static void process_kbd_report(hid_keyboard_report_t const* r1, hid_keyboard_report_t const* r2,
                               void (*kbd_raw_key_cb)(int code)) {
    // Left GUI modifier
    if (r1->modifier & KEYBOARD_MODIFIER_LEFTGUI && !(r2->modifier & KEYBOARD_MODIFIER_LEFTGUI)) {
        kbd_raw_key_cb(HID_KEY_GUI_LEFT | 0x100);
    }

    // Right GUI modifier
    if (r1->modifier & KEYBOARD_MODIFIER_RIGHTGUI && !(r2->modifier & KEYBOARD_MODIFIER_RIGHTGUI)) {
        kbd_raw_key_cb(HID_KEY_GUI_RIGHT | 0x100);
    }

    // Process keycodes
    for (int i = 0; i < 6; i++) {
        if (r1->keycode[i]) {
            if (!find_key_in_report(r2, r1->keycode[i])) {
                bool is_shift = r1->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
                bool is_ctrl = r1->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
                uint8_t keycode = r1->keycode[i];
                int code = conv_table[keycode][is_shift];
                if (code == 0 && keycode != 0) {
                    code = keycode | 0x100;
                } else if (is_ctrl) {
                    code &= ~0x60;
                }
                kbd_raw_key_cb(code);
            }
        }
    }
}

static hid_keyboard_report_t prev_report = {0, 0, {0}};

static void find_pressed_keys(hid_keyboard_report_t const* report) {
    process_kbd_report(report, &prev_report, &kbd_raw_key_down);
}

static void find_released_keys(hid_keyboard_report_t const* report) {
    process_kbd_report(&prev_report, report, &kbd_raw_key_up);
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_len;
    (void)desc_report;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)instance;
    (void)len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            find_pressed_keys((const hid_keyboard_report_t*)report);
            find_released_keys((const hid_keyboard_report_t*)report);
            memcpy(&prev_report, report, sizeof(hid_keyboard_report_t));
            break;
        default:
            break;
    }

    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;
}
