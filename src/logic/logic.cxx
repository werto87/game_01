#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/fusion/include/pair.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <confu_boost/confuBoost.hxx>
#include <crypt.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <pipes/filter.hpp>
#include <pipes/pipes.hpp>
#include <pipes/push_back.hpp>
#include <pipes/transform.hpp>
#include <range/v3/all.hpp>
#include <range/v3/range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/filter.hpp>
#include <sodium.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> overloaded (Ts...) -> overloaded<Ts...>;

boost::asio::awaitable<std::vector<std::string> >
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::map<size_t, User> &users, User &user)
{
  auto result = std::vector<std::string>{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          result.push_back (co_await createAccount (objectAsString, io_context, pool));
        }
      else if (typeToSearch == "LoginAccount")
        {
          result.push_back (co_await loginAccount (objectAsString, io_context, pool, user));
        }
      // broadcast message|channel,msg
      else if (user.accountId && typeToSearch == "BroadcastMessage")
        {
          broadcastMessage (objectAsString, users, user);
        }
      // join channel|channel
      else if (user.accountId && typeToSearch == "JoinChannel")
        {
          result.push_back (joinChannel (objectAsString, user));
        }
      // leave channel|channel
      else if (user.accountId && typeToSearch == "LeaveChannel")
        {
          leaveChannel (objectAsString, user);
        }
      else
        {
          result.push_back ("error|unhandled message: " + msg);
        }
    }
  co_return result;
}

boost::asio::awaitable<std::string>
createAccount (std::string objectAsString, boost::asio::io_context &io_context, boost::asio::thread_pool &pool)
{
  auto createAccountObject = confu_boost::toObject<shared_class::CreateAccount> (objectAsString);
  auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
  if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
    {
      co_return objectToStringWithObjectName (shared_class::CreateAccountSuccess{ .accountId = account->id, .accountName = account->accountName });
    }
  else
    {
      co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "account already created" });
    }
}

boost::asio::awaitable<std::string>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, User &user)
{
  auto loginAccountObject = confu_boost::toObject<shared_class::LoginAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName);
  if (account && co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
    {
      user.accountId = account->id;
      co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ .accountId = account->id, .accountName = loginAccountObject.accountName });
    }
  else
    {
      co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Incorrect username or password" });
    }
}

void
broadcastMessage (std::string const &objectAsString, std::map<size_t, User> &users, User const &sendingUser)
{
  auto broadCastMessageObject = confu_boost::toObject<shared_class::BroadCastMessage> (objectAsString);
  for (auto &[key, user] : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountId = sendingUser.accountId] (auto const &keyUser) {
                             auto &user = std::get<1> (keyUser);
                             return user.accountId != accountId && user.communicationChannels.find (channel) != user.communicationChannels.end ();
                           }))
    {
      auto message = shared_class::Message{ .fromAccount = sendingUser.accountId.value (), .channel = broadCastMessageObject.channel, .message = broadCastMessageObject.message };
      user.msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
    }
}

std::string
joinChannel (std::string const &objectAsString, User &user)
{
  auto joinChannelObject = confu_boost::toObject<shared_class::JoinChannel> (objectAsString);
  user.communicationChannels.insert (joinChannelObject.channel);
  return objectToStringWithObjectName (shared_class::JoinChannelSuccess{ .channel = joinChannelObject.channel });
}

void
leaveChannel (std::string const &objectAsString, User &user)
{
  auto leaveChannelObject = confu_boost::toObject<shared_class::LeaveChannel> (objectAsString);
  user.communicationChannels.erase (leaveChannelObject.channel);
}
