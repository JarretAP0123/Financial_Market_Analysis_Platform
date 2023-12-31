#ifndef PrimaryView_hpp
#define PrimaryView_hpp

#include "view/menu/menu_view.h"
#include "view/view.h"

namespace premia {
class PrimaryView : public View {
 public:
  PrimaryView() = default;

  std::string getName() override;
  void addLogger(const Logger& logger) override;
  void addEvent(const std::string& key, const EventHandler& event) override;
  void Update() override;

 private:
  void DrawInfoPane();
  void DrawScreen();

  Logger logger;
  std::shared_ptr<View> menuView = std::make_shared<MenuView>();
  std::unordered_map<std::string, EventHandler> events;
};
}  // namespace premia

#endif