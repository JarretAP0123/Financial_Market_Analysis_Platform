#ifndef ChartView_hpp
#define ChartView_hpp

#include <memory>
#include <string>

#include "metatypes.h"
#include "view/chart/subview/candle_chart.h"
#include "view/view.h"

namespace premia {
class ChartView : public View {
  using ChartMap = std::unordered_map<std::string, std::shared_ptr<Chart>>;

 public:
  std::string getName() override;
  void addLogger(const Logger& logger) override;
  void addEvent(const std::string& key, const EventHandler& event) override;
  void Update() override;

 private:
  void initChart();
  void DrawChart();
  void DrawChartSettings();

  
  int period_type = 2;
  int period_amount = 0;
  int frequency_type = 1;
  int frequency_amount = 0;
  bool isInit = false;
  std::string tickerSymbol;
  std::string currentChart;

  EventMap events;
  ChartMap charts;
  std::shared_ptr<ChartModel> model = std::make_shared<ChartModel>();

  Logger logger;
};
}  // namespace premia

#endif