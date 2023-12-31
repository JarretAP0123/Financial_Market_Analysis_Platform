#include "menu_view.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <string>

#include "metatypes.h"
#include "model/model.h"
#include "view/core/IconsMaterialDesign.h"
#include "view/view.h"

namespace premia {

void MenuView::DrawFileMenu() {
  static bool show_console = false;

  if (show_console) {
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
    ImGui::Begin("Console", &show_console, ImGuiWindowFlags_NoScrollbar);
    events.at("consoleView")();
    ImGui::End();
  }

  if (ImGui::BeginMenu(ICON_MD_DASHBOARD)) {
    ImGui::MenuItem("New Workspace", "CTRL + N");
    ImGui::Separator();
    ImGui::MenuItem("Open Workspace", "CTRL + O");
    if (ImGui::BeginMenu("Open Recent")) {
      ImGui::MenuItem("None");
      ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::MenuItem("Save Workspace", "CTRL + S");
    ImGui::Separator();
    ImGui::MenuItem("Open Console", ICON_MD_TERMINAL, &show_console);
    ImGui::Separator();
    if (ImGui::BeginMenu("Preferences")) {
      static bool privateBalance = false;
      if (ImGui::MenuItem("Private Balances", "", &privateBalance)) {
        //halext::HLXT::getInstance().setPrivateBalance(privateBalance);
      }
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Quit", "ESC")) {
      events.at("quit")();
    }

    ImGui::EndMenu();
  }
}

void MenuView::DrawTradeMenu() {
  if (ImGui::BeginMenu(ICON_MD_SYNC_ALT)) {
    ImGui::MenuItem("Place Order", "N/A");
    ImGui::MenuItem("Replace Order", "N/A");
    ImGui::MenuItem("Cancel Order", "N/A");
    ImGui::Separator();
    ImGui::MenuItem("Get Order", "N/A");
    ImGui::Separator();
    if (ImGui::MenuItem("Option Chain")) {
      events.at("optionChainView")();
    }
    ImGui::EndMenu();
  }
}

void MenuView::DrawChartsMenu() {
  if (ImGui::BeginMenu(ICON_MD_ADD_CHART)) {
    if (ImGui::MenuItem("Line Plot", ICON_MD_SHOW_CHART)) {
      //halext::HLXT::getInstance().setSelectedChart(0);
      events.at("linePlotView")();
    }
    if (ImGui::MenuItem("Candlestick", ICON_MD_CANDLESTICK_CHART)) {
      //halext::HLXT::getInstance().setSelectedChart(1);
      events.at("chartView")();
    }
    if (ImGui::MenuItem("Multi Plot", ICON_MD_STACKED_LINE_CHART)) {
      //halext::HLXT::getInstance().setSelectedChart(2);
      events.at("chartView")();
    }
    if (ImGui::MenuItem("Advanced", ICON_MD_MULTILINE_CHART)) {
      //halext::HLXT::getInstance().setSelectedChart(3);
      events.at("chartView")();
    }

    if (ImGui::MenuItem("Futures", ICON_MD_AUTO_GRAPH)) {
      //halext::HLXT::getInstance().setSelectedChart(4);
      events.at("chartView")();
    }

    if (ImGui::MenuItem("Crypto", ICON_MD_CURRENCY_BITCOIN)) {
      //halext::HLXT::getInstance().setSelectedChart(5);
      events.at("chartView")();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Movers Up", ICON_MD_TRENDING_UP)) {
      events.at("moversUpView")();
    }

    if (ImGui::MenuItem("Movers Down", ICON_MD_TRENDING_DOWN)) {
      events.at("moversDownView")();
    }

    ImGui::EndMenu();
  }
}

void MenuView::DrawAnalyzeMenu() {
  if (ImGui::BeginMenu(ICON_MD_TOPIC)) {
    if (ImGui::MenuItem("Risk Premia Hub")) events.at("goHome")();
    ImGui::Separator();
    ImGui::MenuItem("Fundamentals", "N/A");
    ImGui::Separator();
    ImGui::MenuItem("Insider Roster", "PRO");
    ImGui::MenuItem("Insider Summary", "PRO");
    ImGui::MenuItem("Insider Transactions", "PRO");
    ImGui::MenuItem("Fund Ownership", "PRO");
    ImGui::Separator();
    ImGui::MenuItem("Retail Money Funds", "PRO");
    ImGui::MenuItem("Institutional Money Funds", "PRO");
    ImGui::MenuItem("Institutional Ownership", "PRO");
    ImGui::Separator();
    ImGui::MenuItem("Largest Trades", "PRO");
    ImGui::MenuItem("Market Volume (U.S.)", "PRO");
    ImGui::Separator();
    ImGui::MenuItem("Daily Treasury Rates", "PRO");
    ImGui::MenuItem("Federal Funds Rate", "PRO");
    ImGui::MenuItem("Unemployment Rate", "PRO");
    ImGui::MenuItem("US Recession Probabilities", "PRO");
    ImGui::Separator();
    ImGui::MenuItem("Consumer Price Index", "PRO");
    ImGui::MenuItem("Industrial Production Index", "PRO");
    ImGui::Separator();
    ImGui::EndMenu();
  }
}

void MenuView::DrawColumnOptions(int x) {
  std::string column = "LeftCol";
  if (x) column = "RightCol";

  if (ImGui::MenuItem("Option Chain")) {
    events.at("optionChain" + column)();
  }
}

void MenuView::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_implot_metrics = false;
  static bool show_imgui_style_editor = false;
  static bool show_implot_style_editor = false;
  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }
  if (show_implot_metrics) {
    ImPlot::ShowMetricsWindow(&show_implot_metrics);
  }
  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }
  if (show_implot_style_editor) {
    ImGui::SetNextWindowSize(ImVec2(415, 762), ImGuiCond_Appearing);
    ImGui::Begin("Style Editor (ImPlot)", &show_implot_style_editor);
    ImPlot::ShowStyleEditor();
    ImGui::End();
  }

  if (ImGui::BeginMenu(ICON_MD_TUNE)) {
    if (ImGui::BeginMenu("Left Column")) {
      DrawColumnOptions(0);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Right Column")) {
      DrawColumnOptions(1);
      ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Appearance")) {
      if (ImGui::MenuItem("Fullscreen")) {
        events.at("toggleFullscreenMode")();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window Layout")) {
      ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::BeginMenu("GUI Tools")) {
      ImGui::MenuItem("Metrics (ImGui)", nullptr, &show_imgui_metrics);
      ImGui::MenuItem("Metrics (ImPlot)", nullptr, &show_implot_metrics);
      ImGui::MenuItem("Style Editor (ImGui)", nullptr,
                      &show_imgui_style_editor);
      ImGui::MenuItem("Style Editor (ImPlot)", nullptr,
                      &show_implot_style_editor);
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void MenuView::DrawHelpMenu() {
  if (ImGui::BeginMenu(ICON_MD_HELP)) {
    ImGui::MenuItem("Get Started");
    ImGui::MenuItem("Tips and Tricks");
    if (ImGui::MenuItem("About")) about = true;

    ImGui::EndMenu();
  }
}

void MenuView::DrawScreen() {
  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawTradeMenu();
    DrawChartsMenu();
    DrawAnalyzeMenu();
    DrawViewMenu();
    DrawHelpMenu();
    ImGui::EndMenuBar();
  }

  if (about) ImGui::OpenPopup("About");

  if (ImGui::BeginPopupModal("About", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Premia Version 0.4");
    ImGui::Text("Written by: Justin Scofield (scawful)");
    ImGui::Text("Dependencies: Boost, SDL2, ImGui, ImPlot");
    ImGui::Text("Data provided by: TDAmeritrade, CoinbasePro");

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      about = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

std::string MenuView::getName() { return "Menu"; }

void MenuView::addLogger(const Logger& newLogger) { this->logger = newLogger; }

void MenuView::addEvent(const std::string& key, const EventHandler& event) {
  this->events[key] = event;
}

void MenuView::Update() { DrawScreen(); }
}  // namespace premia