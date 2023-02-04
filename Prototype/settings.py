import PICOnsole

class Program(PICOnsole.Program):
    def start(self, root_directory, LCD, console_os):
        super().start(root_directory, LCD, console_os)
        self.selection = 0
        return True
    
    def update(self, inputs):
        self.LCD.fill(self.LCD.BLACK)
        title_w = len("Settings") * 8
        center_x = int(self.LCD.width / 2)
        self.LCD.rect(center_x - int(title_w / 2), 6, title_w + 4, 12, self.LCD.WHITE)
        self.LCD.centered_text("Settings", center_x, 8, self.LCD.WHITE)
        
        setting_count = 2
        self.LCD.text("Backlight:{:4.0%}".format(self.console_os.backlight_strength), 12, 20, self.LCD.WHITE)
        if self.selection == 0:
            new_backlight = self.console_os.backlight_strength
            if inputs["right"] == 1:
                new_backlight += 0.01
            elif inputs["right"] % 10 == 1:
                new_backlight += 0.05
                new_backlight = float(int(new_backlight * 20)) / 20
            elif inputs["left"] == 1:
                new_backlight -= 0.01
            elif inputs["left"] % 10 == 1:
                rounded = float(int(new_backlight * 20)) / 20
                if rounded == new_backlight:
                    new_backlight -= 0.05
                else:
                    new_backlight = rounded
            self.console_os.set_backlight_strength(PICOnsole.clamp(new_backlight, 0, 1))
        self.LCD.text("foo", 12, 30, self.LCD.WHITE)
        
        self.LCD.text(">", 2, 20 + self.selection * 10, self.LCD.WHITE)
        
        if inputs["up"] == 1:
            self.selection -= 1
        elif inputs["down"] == 1:
            self.selection += 1
        if self.selection < 0:
            self.selection += setting_count
        elif self.selection >= setting_count:
            self.selection -= setting_count
        
        return True
    
    def stop(self):
        self.console_os.save_settings()

if __name__ == "__main__":
    Program.bootstrap()
