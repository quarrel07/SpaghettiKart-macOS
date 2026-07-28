#pragma once

#include <libultraship.h>
#include "ship/window/gui/Gui.h"
#include "ship/window/Window.h"

namespace Ship {
    class SpaghettiGui : public Gui {
      public:
        SpaghettiGui() : Gui() {}
        SpaghettiGui(std::vector<std::shared_ptr<GuiWindow>> guiWindows) : Gui(guiWindows) {}

      protected:
        void DrawMenu() override;
    };
}