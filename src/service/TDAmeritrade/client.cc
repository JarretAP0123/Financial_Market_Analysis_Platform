#include "client.h"

#include <google/protobuf/message.h>
#include <grpc/support/log.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <boost/asio.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "data/Order.hpp"
#include "data/UserPrincipals.hpp"
#include "handler/tdameritrade_service.h"
#include "parser.h"
#include "socket.h"
#include "src/service/TDAmeritrade/proto/tdameritrade.grpc.pb.h"
#include "src/service/TDAmeritrade/proto/tdameritrade.pb.h"

static auto string_replace(std::string &str, const std::string &from,
                           const std::string &to) -> bool {
  size_t start = str.find(from);
  if (start == std::string::npos) return false;

  str.replace(start, from.length(), to);
  return true;
}

namespace premia {

static size_t json_write(const char *contents, size_t size, size_t nmemb,
                         std::string *s) {
  size_t new_length = size * nmemb;
  try {
    s->append(contents, new_length);
  } catch (const std::bad_alloc &e) {
    // @todo attach a logger
    return EXIT_FAILURE;
  }
  return new_length;
}

namespace tda {

void Client::OpenBrowser() {
  auto host = "http%3A%2F%2Flocalhost:50051";
  auto path = absl::StrCat(
      "https://auth.tdameritrade.com/auth?response_type=code&redirect_uri=",
      host, "&client_id=", absl::StrCat(api_key, "@AMER.OAUTHAP"));
#ifdef _WIN32
  // Note: executable path must use backslashes!
  ::ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWDEFAULT);
#else
#if __APPLE__
  const char *open_executable = "open";
#else
  const char *open_executable = "xdg-open";
#endif
  char command[256];
  snprintf(command, 256, "%s \"%s\"", open_executable, path.c_str());
  system(command);
#endif
}

void Client::CreateChannel(bool has_refresh_token) {
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());
  stub_ = ::TDAmeritrade::NewStub(channel);

  ClientContext ctx_rpc;
  AccessTokenRequest request;
  if (has_refresh_token) {
    request.set_grant_type("refresh_token");
    request.set_refresh_token(refresh_token);
    request.set_client_id(api_key);
  } else {
    request.set_grant_type("authorization_code");
    request.set_access_type("offline");
    request.set_client_id(absl::StrCat(api_key, "@AMER.OAUTHAP"));
  }
  AccessTokenResponse response;
  stub_->PostAccessToken(&ctx_rpc, request, &response);
}

absl::Status Client::PostAccessToken() {
  AccessTokenRequest request;
  request.set_grant_type("refresh_token");
  request.set_refresh_token(refresh_token);
  request.set_client_id(api_key);

  AccessTokenResponse response;
  Status status = stub_->PostAccessToken(&rpc_context, request, &response);

  if (!status.ok()) {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }

  std::cerr << "Successully received access token!" << std::endl;
  access_token = response.access_token();
  refresh_token = response.refresh_token();
  return absl::OkStatus();
}

absl::Status Client::GetAccount(const absl::string_view account_id) {
  AccountRequest account_request;
  account_request.set_accountid(account_id.data());

  AccountResponse account_response;
  Status status =
      stub_->GetAccount(&rpc_context, account_request, &account_response);

  if (!status.ok()) {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }
  std::cerr << "Account Response: " << account_response.SerializeAsString()
            << std::endl;
  return absl::OkStatus();
}

absl::Status Client::GetUserPrincipals() {
  UserPrincipalsRequest request;
  UserPrincipalsResponse response;
  Status status = stub_->GetUserPrincipals(&rpc_context, request, &response);

  if (!status.ok()) {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }
  std::cerr << "User Principals Response: " << response.SerializeAsString()
            << std::endl;
  return absl::OkStatus();
}

absl::Status Client::GetPriceHistory(const std::string &symbol,
                                     PeriodType ptype, int period_amt,
                                     FrequencyType ftype, int freq_amt,
                                     bool ext) {
  PriceHistoryRequest request;
  PriceHistoryResponse response;

  Status status = stub_->GetPriceHistory(&rpc_context, request, &response);

  // Act upon its status.
  if (status.ok()) {
    std::cerr << "Price History Response: " << response.SerializeAsString()
              << std::endl;
    return absl::OkStatus();
  } else {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }
}

absl::Status Client::GetOptionChain(
    absl::string_view ticker, absl::string_view contractType,
    absl::string_view strikeCount, absl::string_view strategy,
    absl::string_view range, absl::string_view expMonth,
    absl::string_view optionType, bool includeQuotes) {
  OptionChainRequest request;
  OptionChainResponse response;

  request.set_symbol(ticker.data());
  request.set_contracttype(contractType.data());
  request.set_strikecount(strikeCount.data());
  request.set_strategy(strategy.data());
  request.set_range(range.data());
  request.set_expmonth(expMonth.data());
  request.set_optiontype(optionType.data());
  request.set_includequotes(includeQuotes);

  Status status = stub_->GetOptionChain(&rpc_context, request, &response);

  if (!status.ok()) {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }
  std::cerr << "Option Chain Response: " << response.SerializeAsString()
            << std::endl;
  return absl::OkStatus();
}

std::string Client::get_api_interval_value(int value) const {
  return EnumAPIValues[value];
}
std::string Client::get_api_frequency_type(int value) const {
  return EnumAPIFreq[value];
}
std::string Client::get_api_period_amount(int value) const {
  return EnumAPIPeriod[value];
}
std::string Client::get_api_frequency_amount(int value) const {
  return EnumAPIFreqAmt[value];
}

// Send a request for data from the API using the json callback
std::string Client::send_request(const std::string &endpoint) const {
  CURL *curl;
  CURLcode res;
  std::string response;

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "premia-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
  res = curl_easy_perform(curl);

  // if (res != CURLE_OK)
  //   throw premia::TDAClientException("send_request() failed",
  //                                    curl_easy_strerror(res));

  curl_easy_cleanup(curl);
  return response;
}

// Send an authorized request for data from the API using the json callback
std::string Client::send_authorized_request(const std::string &endpoint) const {
  CURL *curl;
  CURLcode res;
  CURLHeader headers = nullptr;
  std::string response;
  std::string auth_bearer = "Authorization: Bearer " + access_token;

  curl = curl_easy_init();
  headers = curl_slist_append(headers, auth_bearer.c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "premia-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

  res = curl_easy_perform(curl);

  curl_easy_cleanup(curl);
  return response;
}

// POST Request using access token
void Client::post_authorized_request(const std::string &endpoint,
                                     const std::string &data) const {
  CURL *curl;
  CURLcode res;
  CURLHeader headers = nullptr;
  std::string response;

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, true);

  std::string auth_bearer = "Authorization: Bearer " + access_token;
  curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  curl_slist_append(headers, auth_bearer.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "premia-agent/1.0");

  std::string easy_escape = curl_easy_escape(
      curl, refresh_token.c_str(), static_cast<int>(refresh_token.length()));
  std::string data_post =
      "grant_type=refresh_token&refresh_token=" + easy_escape +
      "&client_id=" + api_key;
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_post.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_post.length());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
}

// Send a POST request using the consumer key and refresh token to get
// the access token
std::string Client::post_access_token() const {
  CURL *curl;
  CURLcode res;
  CURLHeader headers = nullptr;
  std::string response;

  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://api.tdameritrade.com/v1/oauth2/token");
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, true);
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLHeader chunk = nullptr;  // chunked request for http1.1/200 ok
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

  // specify post data, have to url encode the refresh token
  std::string easy_escape = curl_easy_escape(
      curl, refresh_token.c_str(), static_cast<int>(refresh_token.length()));
  std::string data_post =
      "grant_type=refresh_token&refresh_token=" + easy_escape +
      "&client_id=" + api_key;

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_post.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_post.length());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "premia-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

  // run the operations
  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return response;
}

// Get User Principals from API endpoint
// Parse and store in UserPrincipals object for local use
void Client::get_user_principals() {
  std::string endpoint =
      "https://api.tdameritrade.com/v1/"
      "userprincipals?fields=streamerSubscriptionKeys,streamerConnectionInfo";
  std::string response = send_authorized_request(endpoint);
  _user_principals = parser.read_response(response);
  user_principals = parser.parse_user_principals(_user_principals);
  has_user_principals = true;
}

void Client::check_user_principals() {
  if (!has_user_principals) get_user_principals();
}

json::ptree Client::create_login_request() {
  json::ptree credentials;
  json::ptree requests;
  json::ptree parameters;

  check_user_principals();
  BOOST_FOREACH (json::ptree::value_type &v,
                 _user_principals.get_child("accounts.")) {
    for (auto const &acct_it : v.second) {
      account_data[acct_it.first] = acct_it.second.get_value<std::string>();
    }
    break;
  }

  requests.put("service", "ADMIN");
  requests.put("command", "LOGIN");
  requests.put("requestid", 1);
  requests.put("account", account_data["accountId"]);
  requests.put("source", _user_principals.get<std::string>(
                             json::ptree::path_type("streamerInfo.appId")));

  credentials.put("userid", account_data["accountId"]);
  credentials.put("company", account_data["company"]);
  credentials.put("segment", account_data["segment"]);
  credentials.put("cddomain", account_data["accountCdDomainId"]);
  credentials.put("userid", account_data["accountId"]);
  credentials.put("usergroup",
                  _user_principals.get<std::string>(
                      json::ptree::path_type("streamerInfo.userGroup")));
  credentials.put("accesslevel",
                  _user_principals.get<std::string>(
                      json::ptree::path_type("streamerInfo.accessLevel")));
  credentials.put("authorized", "Y");
  credentials.put("acl", _user_principals.get<std::string>(
                             json::ptree::path_type("streamerInfo.acl")));

  std::tm token_timestamp =
      {};  // token timestamp format :: 2021-08-10T14:57:11+0000
  std::string original_token_timestamp =
      _user_principals.get<std::string>("streamerInfo.tokenTimestamp");

  // remove 'T' character
  std::size_t found = original_token_timestamp.find('T');
  std::string reformatted_token_timestamp =
      original_token_timestamp.replace(found, 1, " ");

  // remove the UTC +0000 portion, will adjust for this manually
  found = reformatted_token_timestamp.find('+');
  reformatted_token_timestamp =
      reformatted_token_timestamp.replace(found, 5, " ");

  // convert string timestamp into time_t
  std::istringstream ss(reformatted_token_timestamp);
  ss >> std::get_time(&token_timestamp, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    std::cout << "Token timestamp parse failed!" << std::endl;
  } else {
    // this is disgusting i'm sorry
    std::time_t token_timestamp_as_sec = std::mktime(&token_timestamp);
    std::chrono::time_point token_timestamp_point =
        std::chrono::system_clock::from_time_t(token_timestamp_as_sec);
    auto duration = token_timestamp_point.time_since_epoch();
    auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    millis -= 18000000;
    millis -= 3600000;
    credentials.put("timestamp", millis);
  }

  credentials.put("appid", _user_principals.get<std::string>(
                               json::ptree::path_type("streamerInfo.appId")));

  std::string credential_str;  // format parameters
  for (const auto &[key, value] : (json::ptree)credentials) {
    credential_str += key + "%3D" + value.get_value<std::string>() + "%26";
  }
  std::size_t end = credential_str.size();
  credential_str.replace(end - 3, 3, "");

  parameters.put("token", _user_principals.get<std::string>(
                              json::ptree::path_type("streamerInfo.token")));
  parameters.put("version", "1.0");
  parameters.put("credential", credential_str);

  requests.add_child("parameters", parameters);  // include in requests
  return requests;
}

json::ptree Client::create_logout_request() {
  json::ptree requests;
  json::ptree parameters;
  requests.put("service", "ADMIN");
  requests.put("requestid", 3);
  requests.put("command", "LOGOUT");
  requests.put("account", account_data["accountId"]);
  requests.put("source", _user_principals.get<std::string>(
                             json::ptree::path_type("streamerInfo.appId")));
  requests.add_child("parameters", parameters);
  return requests;
}

json::ptree Client::create_service_request(ServiceType type,
                                           const std::string &keys,
                                           const std::string &fields) {
  json::ptree requests;
  json::ptree parameters;
  requests.put("service", EnumAPIServiceName[type]);
  requests.put("requestid", 2);
  requests.put("command", "SUBS");
  requests.put("account", account_data["accountId"]);
  requests.put("source", _user_principals.get<std::string>(
                             json::ptree::path_type("streamerInfo.appId")));
  parameters.put("keys", keys);
  parameters.put("fields", fields);
  requests.add_child("parameters", parameters);
  return requests;
}

Client::Client() { curl_global_init(CURL_GLOBAL_SSL); }
Client::~Client() { curl_global_cleanup(); }

// Start a WebSocket session quickly with a ticker and fields
void Client::start_session(const std::string &ticker) {
  std::string host;
  std::string port = "443";
  std::string all_requests;
  json::ptree requests;
  std::stringstream requests_text_stream;
  std::vector<json::ptree> requests_array;

  host = _user_principals.get<std::string>("streamerInfo.streamerSocketUrl");

  requests_array.push_back(create_login_request());
  requests_array.push_back(
      create_service_request(QUOTE, ticker, "0,1,2,3,4,5,6,7,8"));
  requests = bind_requests(requests_array);
  write_json(requests_text_stream, requests);
  all_requests = requests_text_stream.str();
  websocket_session = std::make_shared<tda::Socket>(ioc_pool.get_executor(),
                                                    context, all_requests);
  websocket_session->open(host.c_str(), port.c_str());
}

// WebSocket session logout request
void Client::send_logout_request() {
  json::ptree logout_request = create_logout_request();
  std::stringstream logout_text_stream;
  json::write_json(logout_text_stream, logout_request);
  std::string logout_text = logout_text_stream.str();
  websocket_session->write(logout_text);
  websocket_session->close();
}

// API Access Token retrieval
void Client::fetch_access_token() {
  access_token = parser.parse_access_token(post_access_token());
  has_access_token = true;
}

// Request account data by the account id
// Return the API response after authorization

std::string Client::get_account(const std::string &account_id) {
  check_user_principals();
  std::string account_url =
      "https://api.tdameritrade.com/v1/accounts/"
      "{accountNum}?fields=positions,orders";
  string_replace(account_url, "{accountNum}", account_id);
  return send_authorized_request(account_url);
}

// Get all account data as a response
std::string Client::get_all_accounts() {
  check_user_principals();
  std::string account_url =
      "https://api.tdameritrade.com/v1/accounts/?fields=positions,orders";
  return send_authorized_request(account_url);
}

// Create a vector of all the account ids present on the API key
std::vector<std::string> Client::get_all_account_ids() {
  std::vector<std::string> accounts;

  for (const auto &[key, value] : _user_principals) {
    if (key == "accounts") {
      for (const auto &[key2, val2] : value) {
        for (const auto &[elementKey, elementValue] : val2) {
          if (elementKey == "accountId") {
            accounts.push_back(elementValue.get_value<std::string>());
          }
        }
      }
    }

    if (key == "primaryAccountId") {
      std::cout << "Here" << value.get_value<std::string>() << std::endl
                << std::flush;
      accounts.push_back(value.get_value<std::string>());
    }
  }

  return accounts;
}

// Request quote data by the instrument symbol
// Return the API response
std::string Client::get_quote(const std::string &symbol) const {
  std::string url =
      "https://api.tdameritrade.com/v1/marketdata/{ticker}/quotes?apikey=" +
      api_key;
  string_replace(url, "{ticker}", symbol);
  return send_request(url);
}

// Prepare a request for watchlist data by an account number
// Return the API response
std::string Client::get_watchlist_by_account(
    const std::string &account_id) const {
  std::string url =
      "https://api.tdameritrade.com/v1/accounts/{accountNum}/watchlists";
  string_replace(url, "{accountNum}", account_id);
  return send_authorized_request(url);
}

// Prepare a request from the API for price history information
//        Return the API response
std::string Client::get_price_history(const std::string &symbol,
                                      PeriodType ptype, int period_amt,
                                      FrequencyType ftype, int freq_amt,
                                      bool ext) const {
  std::string url =
      "https://api.tdameritrade.com/v1/marketdata/{ticker}/"
      "pricehistory?apikey=" +
      api_key +
      "&periodType={periodType}&period={period}&frequencyType={frequencyType}&"
      "frequency={frequency}&needExtendedHoursData={ext}";

  string_replace(url, "{ticker}", symbol);
  string_replace(url, "{periodType}", get_api_interval_value(ptype));
  string_replace(url, "{period}", get_api_period_amount(period_amt));
  string_replace(url, "{frequencyType}", get_api_frequency_type(ftype));
  string_replace(url, "{frequency}", get_api_frequency_amount(freq_amt));

  if (!ext)
    string_replace(url, "{ext}", "false");
  else
    string_replace(url, "{ext}", "true");

  return send_request(url);
}

// Prepare a request from the API for option chain data
// Return the API response
std::string Client::get_option_chain(
    const std::string &ticker, const std::string &contractType,
    const std::string &strikeCount, bool includeQuotes,
    const std::string &strategy, const std::string &range,
    const std::string &expMonth, const std::string &optionType) const {
  OptionChain option_chain;
  std::string url =
      "https://api.tdameritrade.com/v1/marketdata/chains?apikey=" + api_key +
      "&symbol={ticker}&contractType={contractType}&strikeCount={strikeCount}&"
      "includeQuotes={includeQuotes}&strategy={strategy}&range={range}&"
      "expMonth={expMonth}&optionType={optionType}";

  string_replace(url, "{ticker}", ticker);
  string_replace(url, "{contractType}", contractType);
  string_replace(url, "{strikeCount}", strikeCount);
  string_replace(url, "{strategy}", strategy);
  string_replace(url, "{range}", range);
  string_replace(url, "{expMonth}", expMonth);
  string_replace(url, "{optionType}", optionType);

  if (!includeQuotes)
    string_replace(url, "{includeQuotes}", "FALSE");
  else
    string_replace(url, "{includeQuotes}", "TRUE");

  return send_request(url);
}

// Retrieve order by the account and the order id
std::string Client::get_order(const std::string &account_id,
                              const std::string &order_id) const {
  std::string endpoint =
      "https://api.tdameritrade.com/v1/accounts/{accountId}/orders/{orderId}";
  string_replace(endpoint, "{accountId}", account_id);
  string_replace(endpoint, "{orderId}", order_id);
  return send_authorized_request(endpoint);
}

// Retrieve Order using query parameters
std::string Client::get_orders_by_query(const std::string &account_id,
                                        int maxResults, double fromEnteredTime,
                                        double toEnteredTime,
                                        OrderStatus status) const {
  std::string endpoint =
      "https://api.tdameritrade.com/v1/accounts/{accountId}/"
      "orders?maxResults={maxResults}&fromEnteredTime={from}&toEnteredTime={to}"
      "&status={status}";
  string_replace(endpoint, "{accountId}", account_id);
  string_replace(endpoint, "{maxResults}", std::to_string(maxResults));
  string_replace(endpoint, "{from}", std::to_string(fromEnteredTime));
  string_replace(endpoint, "{to}", std::to_string(toEnteredTime));
  string_replace(endpoint, "{status}", "status");
  return send_authorized_request(endpoint);
}

// Place an Order for the account by id
void Client::place_order(const std::string &account_id,
                         const Order &order) const {
  std::string endpoint =
      "https://api.tdameritrade.com/v1/accounts/{accountId}/orders";
  string_replace(endpoint, "{accountId}", account_id);
  post_authorized_request(endpoint, order.getString());
}

// temp function for passing key to client
void Client::addAuth(const std::string &key, const std::string &token) {
  api_key = key;
  refresh_token = token;
}

}  // namespace tda
}  // namespace premia