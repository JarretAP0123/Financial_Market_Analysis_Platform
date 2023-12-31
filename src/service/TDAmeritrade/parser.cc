#include "parser.h"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>

#include "data/Account.hpp"
#include "data/OptionChain.hpp"
#include "data/PriceHistory.hpp"
#include "data/PricingStructures.hpp"
#include "data/Quote.hpp"
#include "data/UserPrincipals.hpp"
#include "data/Watchlist.hpp"

namespace premia {
namespace tda {

/**
 * @brief
 *
 * @param data
 * @param chain
 * @param idx
 */
void Parser::parseStrikeMap(const json::ptree &data, OptionChain &chain,
                            int idx) const {
  for (const auto &[dateKey, dateValue] : data) {
    OptionsDateTimeObj options_dt_obj;
    options_dt_obj.datetime = dateKey;
    for (const auto &[strikeKey, strikeValue] : dateValue) {
      StrikePriceMap imported_strike;
      imported_strike.strikePrice = strikeKey;
      for (const auto &[contractKey, contractValue] : strikeValue) {
        for (const auto &[detailsKey, detailsValue] : contractValue) {
          imported_strike.raw_option[detailsKey] =
              detailsValue.get_value<std::string>();
        }
        options_dt_obj.strikePriceObj.push_back(imported_strike);
      }
    }
    chain.addOptionsDateTimeObj(options_dt_obj);
    chain.addOptionsDateTimeObj(options_dt_obj, idx);
  }
}

/**
 * @brief Take a response from the API as argument and read it into a
 * boost::property_tree
 * @author @scawful
 *
 * @param response
 * @return json::ptree
 */
json::ptree Parser::read_response(const std::string &response) const {
  std::istringstream json_response(response);
  json::ptree property_tree;
  try {
    read_json(json_response, property_tree);
  } catch (json::ptree_error const &json_parser_error) {
    std::cout << "Parser::read_response: " << json_parser_error.what()
              << std::endl;
  }
  return property_tree;
}

/**
 * @brief Return a human friendly string of the option contract
 *
 * @param symbol
 * @return std::string
 */
std::string Parser::parse_option_symbol(const std::string &symbol) const {
  std::string underlying;
  std::string month;
  std::string day;
  std::string year;
  std::string strike;
  std::string type;

  auto start = symbol.find('_');
  underlying = symbol.substr(0, start);
  month = symbol.substr(start + 1, 2);
  month = months[std::stoi(month)];
  day = symbol.substr(start + 3, 2);
  year = symbol.substr(start + 5, 2);
  type = symbol.substr(start + 7, 1);
  strike = symbol.substr(start + 8, symbol.size()) + type;

  std::string date = day + " " + month + " " + year;
  std::string output = date + "\n" + underlying + " " + strike;

  return output;
}

/**
 * @brief Parse the access token from a response
 * @author @scawful
 *
 * @param response
 * @return std::string
 */
std::string Parser::parse_access_token(const std::string &response) const {
  std::string access_token;
  json::ptree data = read_response(response);
  for (const auto &[key, value] : data) {
    if (key == "access_token") {
      access_token = value.get_value<std::string>();
    }
  }
  return access_token;
}

/**
 * @brief Parse a Quote from the API
 * @author @scawful
 *
 * @param data
 * @return Quote
 */
Quote Parser::parse_quote(const json::ptree &data) const {
  Quote quote;

  for (const auto &[key, value] : data) {
    for (const auto &[propertyKey, propertyValue] : value) {
      quote.setQuoteVariable(propertyKey,
                             propertyValue.get_value<std::string>());
    }
  }

  return quote;
}

/**
 * @brief Parse the price history data from the server
 * @author @scawful
 * @note dt format %a %d %b %Y - %I:%M:%S%p,    %H:%M:%S
 *
 * @param data
 * @param ticker
 * @param freq
 * @return PriceHistory
 */
PriceHistory Parser::parse_price_history(const json::ptree &data,
                                         const std::string &ticker,
                                         int freq) const {
  PriceHistory price_history;
  price_history.setTickerSymbol(ticker);

  for (const auto &[historyKey, historyValue] : data) {
    if (historyKey == "candles") {
      for (const auto &[candleKey, candleValue] : historyValue) {
        tda::Candle newCandle;
        std::string datetime;
        for (const auto &[valueKey, finalValue] : candleValue) {
          try {
            if (valueKey == "open") {
              newCandle.open = boost::lexical_cast<double>(
                  finalValue.get_value<std::string>());
            } else if (valueKey == "close") {
              newCandle.close = boost::lexical_cast<double>(
                  finalValue.get_value<std::string>());
            } else if (valueKey == "high") {
              newCandle.high = boost::lexical_cast<double>(
                  finalValue.get_value<std::string>());
            } else if (valueKey == "low") {
              newCandle.low = boost::lexical_cast<double>(
                  finalValue.get_value<std::string>());
            } else if (valueKey == "volume") {
              newCandle.volume = boost::lexical_cast<double>(
                  finalValue.get_value<std::string>());
            } else if (valueKey == "datetime") {
              std::stringstream dt_ss;
              std::time_t secsSinceEpoch = boost::lexical_cast<std::time_t>(
                  finalValue.get_value<std::string>());
              newCandle.raw_datetime = secsSinceEpoch;
              secsSinceEpoch *= (time_t)0.001;
              dt_ss << std::put_time(std::localtime(&secsSinceEpoch),
                                     "%a %d %b %Y - %I:%M:%S%p");
              datetime = dt_ss.str();
            }
          } catch (const boost::wrapexcept<boost::bad_lexical_cast> &e) {
            std::cout << "parse_price_history:: " << e.what() << std::endl;
          }
        }
        newCandle.datetime = datetime;
        price_history.addCandleByType(newCandle, freq);
        price_history.addCandle(newCandle);
      }
    }
  }
  price_history.setInitialized();
  return price_history;
}

/**
 * @brief Parse the current user principals from the API
 * @author @scawful
 *
 * @param data
 * @return UserPrincipals
 */
UserPrincipals Parser::parse_user_principals(json::ptree &data) const {
  UserPrincipals user_principals;

  try {
    BOOST_FOREACH (json::ptree::value_type &v, data.get_child("accounts.")) {
      std::unordered_map<std::string, std::string> account_data;
      for (const auto &[acctKey, acctValue] : v.second) {
        account_data[acctKey] = acctValue.get_value<std::string>();
      }
      user_principals.add_account_data(account_data);
    }
    user_principals.set_account_data(user_principals.get_account_data_array(0));
  } catch (json::ptree_bad_path) {
    // lazy fix to the account issue
    for (const auto &[key, value] : data) {
      std::unordered_map<std::string, std::string> account_data;
      if (key == "primaryAccountId") {
        account_data[key] = value.get_value<std::string>();
      }
      user_principals.add_account_data(account_data);
    }
    user_principals.set_account_data(user_principals.get_account_data_array(0));
  }

  return user_principals;
}

/**
 * @brief Parse the option chain data from the API
 * @author @scawful
 *
 * @param data
 * @return OptionChain
 */
OptionChain Parser::parse_option_chain(const json::ptree &data) const {
  OptionChain optionChain;
  for (const auto &[optionsKey, optionsValue] : data) {
    if (optionsKey == "callExpDateMap") {
      parseStrikeMap(optionsValue, optionChain, 1);
    }

    if (optionsKey == "putExpDateMap") {
      parseStrikeMap(optionsValue, optionChain, 0);
    }

    if (optionsKey == "underlying") {
      for (const auto &[underlyingKey, underlyingValue] : optionsValue) {
        optionChain.setUnderlyingVariable(
            underlyingKey, underlyingValue.get_value<std::string>());
      }
    }

    optionChain.setOptionChainVariable(optionsKey,
                                       optionsValue.get_value<std::string>());
  }
  return optionChain;
}

/**
 * @brief Parse the users account data from the API
 * @author @scawful
 *
 * @param data
 * @return Account
 */
Account Parser::parse_account(const json::ptree &data) const {
  Account account;
  for (const auto &[classKey, classValue] : data) {
    for (const auto &[accountKey, accountValue] : classValue) {
      if (accountKey == "positions") {
        for (const auto &[positionListKey, positionListValue] : accountValue) {
          tda::PositionBalances new_position_balance;  // positions and balances
          for (const auto &[positionsKey, positionsValue] : positionListValue) {
            new_position_balance.balances[positionsKey] =
                positionsValue.get_value<std::string>();
            std::unordered_map<std::string, std::string> pos_field;
            std::unordered_map<std::string, std::string> instrument;
            for (const auto &[fieldKey, fieldValue] : positionsValue) {
              if (fieldKey == "symbol") {
                new_position_balance.symbol =
                    fieldValue.get_value<std::string>();
              }
              pos_field[fieldKey] = fieldValue.get_value<std::string>();
            }
            account.add_position(pos_field);
          }
          account.add_balance(new_position_balance);
        }
      } else if (accountKey == "currentBalances") {
        for (const auto &[balanceKey, balanceValue] : accountValue) {
          account.set_balance_variable(balanceKey,
                                       balanceValue.get_value<std::string>());
        }
      } else {
        account.set_account_variable(accountKey,
                                     accountValue.get_value<std::string>());
      }
    }
  }
  return account;
}

/**
 * @brief Parse the users account data from the API
 * @author @scawful
 *
 * @param data
 * @return Account
 */
Account Parser::parse_all_accounts(const json::ptree &data) const {
  Account account;
  for (const auto &[key, val] : data) {
    for (const auto &[classKey, classValue] : val) {
      for (const auto &[accountKey, accountValue] : classValue) {
        if (accountKey == "positions") {
          for (const auto &[positionListKey, positionListValue] :
               accountValue) {
            tda::PositionBalances
                new_position_balance;  // positions and balances
            for (const auto &[positionsKey, positionsValue] :
                 positionListValue) {
              std::cout << "Positions List: " << positionListKey << " : "
                        << positionListValue.get_value<std::string>()
                        << std::endl;
              new_position_balance.balances[positionsKey] =
                  positionsValue.get_value<std::string>();
              std::unordered_map<std::string, std::string> pos_field;
              std::unordered_map<std::string, std::string> instrument;
              for (const auto &[fieldKey, fieldValue] : positionsValue) {
                if (fieldKey == "symbol") {
                  new_position_balance.symbol =
                      fieldValue.get_value<std::string>();
                }
                pos_field[fieldKey] = fieldValue.get_value<std::string>();
              }
              account.add_position(pos_field);
            }
            account.add_balance(new_position_balance);
          }
        } else if (accountKey == "currentBalances") {
          for (const auto &[balanceKey, balanceValue] : accountValue) {
            account.set_balance_variable(balanceKey,
                                         balanceValue.get_value<std::string>());
          }
        } else {
          account.set_account_variable(accountKey,
                                       accountValue.get_value<std::string>());
        }
      }
    }
  }
  return account;
}

/**
 * @brief Parse account watchlist data from the API
 * @author @scawful
 *
 * @param data
 * @return std::vector<tda::Watchlist>
 */
std::vector<tda::Watchlist> Parser::parse_watchlist_data(
    const json::ptree &data) const {
  std::vector<tda::Watchlist> watchlists;
  for (const auto &[dataKey, dataValue] : data) {
    Watchlist watchlist;
    try {
      watchlist.setName(dataValue.get_child("name").get_value<std::string>());
      watchlist.setId(dataValue.get_child("watchlistId").get_value<int>());
      watchlist.setAccountId(
          dataValue.get_child("accountId").get_value<std::string>());
    } catch (const json::ptree_error &e) {
      std::cout << e.what() << std::endl;
    }

    for (const auto &[itemKey, itemValue] :
         dataValue.get_child("watchlistItems")) {
      for (const auto &[item2Key, item2Value] : itemValue) {
        if (item2Key == "instrument") {
          try {
            std::string symbol =
                item2Value.get_child("symbol").get_value<std::string>();
            std::string desc = "";
            std::string type =
                item2Value.get_child("assetType").get_value<std::string>();
            watchlist.addInstrument(symbol, desc, type);
          } catch (const json::ptree_error &e) {
            std::cout << e.what() << std::endl;
          }
        } else {
          watchlist.addVariable(item2Key, itemValue.get_value<std::string>());
        }
      }
    }
    watchlists.push_back(watchlist);
  }

  return watchlists;
}

}  // namespace tda
}  // namespace premia