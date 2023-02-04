from machine import Pin

import PICOnsole

if __name__=='__main__':
    backlight = Pin(13, Pin.OUT)
    backlight.off()
    del backlight
    picos = None
    try:
        picos = PICOnsole.OS()
        picos.set_backlight_strength(0.5)
        picos.start_builtin_program("folder_view")
        while picos.is_active():
            picos.update()
    except Exception as e:
        if picos != None:
            picos.set_backlight_strength(0)
        raise e
    if picos != None:
        picos.set_backlight_strength(0)

