#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include "src/util.hxx"
#include <algorithm>
#include <bits/ranges_algo.h>
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
#include <boost/serialization/vector.hpp>
#include <boost/type_index.hpp>
#include <confu_json/confu_json.hxx>
#include <crypt.h>
#include <durak/game.hxx>
#include <durak/gameData.hxx>
#include <durak/gameOption.hxx>
#include <fmt/core.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <iterator>
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
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> overloaded (Ts...) -> overloaded<Ts...>;

boost::asio::awaitable<std::vector<std::string>>
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  auto result = std::vector<std::string>{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      // TODO rework this. we dont need result we can put in user in every function and add the messages there. this will help for cases when we have to send messages to multiple users. bz improving symetrie
      // we have 3 cases send something to user send somthing to many users dont send anything.
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          result.push_back (co_await createAccountAndLogin (objectAsString, io_context, user, pool));
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "LoginAccount")
        {
          result.push_back (co_await loginAccount (objectAsString, io_context, users, user, pool, gameLobbys));
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "BroadCastMessage")
        {
          if (auto broadCastMessageResult = broadCastMessage (objectAsString, users, *user))
            {
              result.push_back (broadCastMessageResult.value ());
            }
        }
      else if (typeToSearch == "JoinChannel")
        {
          result.push_back (joinChannel (objectAsString, *user));
        }
      else if (user->accountName && typeToSearch == "LeaveChannel")
        {
          result.push_back (leaveChannel (objectAsString, *user));
        }
      else if (typeToSearch == "LogoutAccount")
        {
          result.push_back (logoutAccount (*user));
        }
      else if (typeToSearch == "CreateGameLobby")
        {
          result = createGameLobby (objectAsString, user, gameLobbys);
        }
      else if (typeToSearch == "JoinGameLobby")
        {
          if (auto joinGameLobbyError = joinGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (joinGameLobbyError.value ());
            }
        }
      else if (typeToSearch == "SetMaxUserSizeInCreateGameLobby")
        {
          if (auto setMaxUserSizeInCreateGameError = setMaxUserSizeInCreateGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (setMaxUserSizeInCreateGameError.value ());
            }
        }
      else if (typeToSearch == "SetMaxCardValueInCreateGameLobby")
        {
          if (auto setMaxCardValueInCreateGameLobbyError = setMaxCardValueInCreateGameLobby (objectAsString, user, gameLobbys))
            {
              result.push_back (setMaxCardValueInCreateGameLobbyError.value ());
            }
        }
      else if (typeToSearch == "LeaveGameLobby")
        {
          if (auto leaveGameLobbyResult = leaveGameLobby (user, gameLobbys))
            {
              result.push_back (leaveGameLobbyResult.value ());
            }
        }
      else if (typeToSearch == "RelogTo")
        {
          if (auto relogToResult = relogTo (objectAsString, user, gameLobbys))
            {
              result.push_back (relogToResult.value ());
            }
        }
      else if (typeToSearch == "LoginAccountCancel")
        {
          loginAccountCancel (user);
        }
      else if (typeToSearch == "CreateAccountCancel")
        {
          createAccountCancel (user);
        }
      else if (typeToSearch == "CreateGame")
        {
          createGame (user, gameLobbys, gameMachines);
        }
      else if (typeToSearch == "DurakAttack")
        {
          durakAttack (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefend")
        {
          durakDefend (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefendPass")
        {
          durakDefendPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAttackPass")
        {
          durakAttackPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAssistPass")
        {
          durakAssistPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAskDefendWantToTakeCardsAnswer")
        {
          durakAskDefendWantToTakeCardsAnswer (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakLeaveGame")
        {
          durakLeaveGame (user, gameMachines);
        }
      else
        {
          std::cout << "could not find a match for typeToSearch '" << typeToSearch << "'" << std::endl;
          result.push_back ("error|unhandled message: " + msg);
        }
    }
  co_return result;
}

boost::asio::awaitable<std::string>
createAccountAndLogin (std::string objectAsString, boost::asio::io_context &io_context, std::shared_ptr<User> user, boost::asio::thread_pool &pool)
{
  auto createAccountObject = stringToObject<shared_class::CreateAccount> (objectAsString);
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
    {
      co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "account already created" });
    }
  else
    {
      auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
      if (user->ignoreCreateAccount)
        {
          co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "Canceled by User Request" });
        }
      else
        {
          if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
            {
              user->accountName = account->accountName;
              co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ .accountName = createAccountObject.accountName });
            }
          else
            {
              co_return objectToStringWithObjectName (shared_class::CreateAccountError{ .accountName = createAccountObject.accountName, .error = "account already created" });
            }
        }
    }
}

boost::asio::awaitable<std::string>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbys)
{
  auto loginAccountObject = stringToObject<shared_class::LoginAccount> (objectAsString);
  if (not user->accountName)
    {
      soci::session sql (soci::sqlite3, pathToTestDatabase);
      if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
        {
          if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
            {
              co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Account already logged in" });
            }
          else
            {
              if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
                {
                  if (user->ignoreLogin)
                    {
                      co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Canceled by User Request" });
                    }
                  else
                    {
                      user->accountName = account->accountName;
                      if (auto gameLobbyWithUser = std::ranges::find_if (gameLobbys,
                                                                         [accountName = user->accountName] (auto const &gameLobby) {
                                                                           auto const &accountNames = gameLobby.accountNames ();
                                                                           return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                         });
                          gameLobbyWithUser != gameLobbys.end ())
                        {
                          co_return objectToStringWithObjectName (shared_class::WantToRelog{ .accountName = loginAccountObject.accountName, .destination = "Create Game Lobby" });
                        }
                      else
                        {
                          co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ .accountName = loginAccountObject.accountName });
                        }
                    }
                }
              else
                {
                  co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Incorrect Username or Password" });
                }
            }
        }
      else
        {
          co_return objectToStringWithObjectName (shared_class::LoginAccountError{ .accountName = loginAccountObject.accountName, .error = "Incorrect username or password" });
        }
    }
  else
    {
      if (auto gameLobbyWithUser = std::ranges::find_if (gameLobbys,
                                                         [accountName = user->accountName] (auto const &gameLobby) {
                                                           auto const &accountNames = gameLobby.accountNames ();
                                                           return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                         });
          gameLobbyWithUser != gameLobbys.end ())
        {
          co_return objectToStringWithObjectName (shared_class::WantToRelog{ .accountName = loginAccountObject.accountName, .destination = "Create Game Lobby" });
        }
      else
        {
          co_return objectToStringWithObjectName (shared_class::LoginAccountSuccess{ .accountName = loginAccountObject.accountName });
        }
    }
}

std::string
logoutAccount (User &user)
{
  user.accountName = {};
  user.msgQueue.clear ();
  user.communicationChannels.clear ();
  user.ignoreLogin = false;
  user.ignoreCreateAccount = false;
  return objectToStringWithObjectName (shared_class::LogoutAccountSuccess{});
}

std::optional<std::string>
broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User>> &users, User const &sendingUser)
{
  auto broadCastMessageObject = stringToObject<shared_class::BroadCastMessage> (objectAsString);
  if (sendingUser.accountName)
    {
      for (auto &user : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = sendingUser.accountName] (auto const &user) { return user->communicationChannels.find (channel) != user->communicationChannels.end (); }))
        {
          soci::session sql (soci::sqlite3, pathToTestDatabase);
          auto message = shared_class::Message{ .fromAccount = sendingUser.accountName.value (), .channel = broadCastMessageObject.channel, .message = broadCastMessageObject.message };
          user->msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
        }
      return {};
    }
  else
    {
      return objectToStringWithObjectName (shared_class::BroadCastMessageError{ .channel = broadCastMessageObject.channel, .error = "account not logged in" });
    }
}

std::string
joinChannel (std::string const &objectAsString, User &user)
{
  auto joinChannelObject = stringToObject<shared_class::JoinChannel> (objectAsString);
  if (user.accountName)
    {
      user.communicationChannels.insert (joinChannelObject.channel);
      return objectToStringWithObjectName (shared_class::JoinChannelSuccess{ .channel = joinChannelObject.channel });
    }
  else
    {
      return objectToStringWithObjectName (shared_class::JoinChannelError{ .channel = joinChannelObject.channel, .error = { "user not logged in" } });
    }
}

std::string
leaveChannel (std::string const &objectAsString, User &user)
{
  auto leaveChannelObject = stringToObject<shared_class::LeaveChannel> (objectAsString);
  if (user.accountName)
    {
      if (user.communicationChannels.erase (leaveChannelObject.channel))
        {
          return objectToStringWithObjectName (shared_class::LeaveChannelSuccess{ .channel = leaveChannelObject.channel });
        }
      else
        {
          return objectToStringWithObjectName (shared_class::LeaveChannelError{ .channel = leaveChannelObject.channel, .error = { "channel not found" } });
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::LeaveChannelError{ .channel = leaveChannelObject.channel, .error = { "user not logged in" } });
    }
}

void
createGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines)
{
  if (auto gameLobbyWithUser = std::ranges::find_if (gameLobbys,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
      gameLobbyWithUser != gameLobbys.end ())
    {
      if (gameLobbyWithUser->gameLobbyAdminAccountName () == user->accountName)
        {
          std::ranges::for_each (gameLobbyWithUser->_users, [] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (shared_class::StartGame{})); });
          auto names = std::vector<std::string>{};
          std::ranges::transform (gameLobbyWithUser->_users, std::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
          auto game = durak::Game{ std::move (names), gameLobbyWithUser->gameOption };
          auto &gameMachine = gameMachines.emplace_back (game, gameLobbyWithUser->_users);
          sendGameDataToAccountsInGame (gameMachine.getGame (), gameLobbyWithUser->_users);
          gameLobbys.erase (gameLobbyWithUser);
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ .error = "you need to be admin in a game lobby to start a game" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ .error = "you need to be admin in a game lobby to start a game" }));
    }
}

std::vector<std::string>
createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto createGameLobbyObject = stringToObject<shared_class::CreateGameLobby> (objectAsString);
  if (std::ranges::find_if (gameLobbys, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == lobbyPassword; }) == gameLobbys.end ())
    {
      if (auto gameLobbyWithUser = std::ranges::find_if (gameLobbys,
                                                         [accountName = user->accountName] (auto const &gameLobby) {
                                                           auto const &accountNames = gameLobby.accountNames ();
                                                           return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                         });
          gameLobbyWithUser != gameLobbys.end ())
        {
          return { objectToStringWithObjectName (shared_class::CreateGameLobbyError{ .error = { "account has already a game lobby with the name: " + gameLobbyWithUser->gameLobbyName () } }) };
        }
      else
        {
          auto &newGameLobby = gameLobbys.emplace_back (GameLobby{ ._name = createGameLobbyObject.name, ._password = createGameLobbyObject.password });
          if (newGameLobby.tryToAddUser (user))
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.gameLobbyName ();
              usersInGameLobby.durakGameOption.maxCardValue = newGameLobby.gameOption.maxCardValue;
              std::ranges::transform (newGameLobby.accountNames (), std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ .accountName = accountName }; });
              result.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
              result.push_back (objectToStringWithObjectName (usersInGameLobby));
              return result;
            }
          else
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
        }
    }
  else
    {
      return { objectToStringWithObjectName (shared_class::CreateGameLobbyError{ .error = { "lobby already exists with name: " + createGameLobbyObject.name } }) };
    }
}

std::optional<std::string>
joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto joinGameLobbyObject = stringToObject<shared_class::JoinGameLobby> (objectAsString);
  if (auto gameLobby = std::ranges::find_if (gameLobbys, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.gameLobbyName () == gameLobbyName && _gameLobby.gameLobbyPassword () == lobbyPassword; }); gameLobby != gameLobbys.end ())
    {
      if (gameLobby->tryToAddUser (user))
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->gameLobbyName ();
          usersInGameLobby.durakGameOption.maxCardValue = gameLobby->gameOption.maxCardValue;
          std::ranges::transform (gameLobby->accountNames (), std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ .accountName = accountName }; });
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          return {};
        }
      else
        {
          return objectToStringWithObjectName (shared_class::JoinGameLobbyError{ .name = joinGameLobbyObject.name, .error = "lobby full" });
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::JoinGameLobbyError{ .name = joinGameLobbyObject.name, .error = "wrong password name combination or lobby does not exists" });
    }
}

std::optional<std::string>
setMaxUserSizeInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto setMaxUserSizeInCreateGameLobbyObject = stringToObject<shared_class::SetMaxUserSizeInCreateGameLobby> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = std::ranges::find_if (gameLobbys,
                                                        [accountName = user->accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames ();
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                        });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
        {
          return objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ .error = errorMessage.value () });
        }
      else
        {
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxUserSizeInCreateGameLobby{ .maxUserSize = setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
          return {};
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ .error = "could not find a game lobby for account" });
    }
}

std::optional<std::string>
setMaxCardValueInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  auto setMaxCardValueInCreateGameLobbyObject = stringToObject<shared_class::SetMaxCardValueInCreateGameLobby> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = std::ranges::find_if (gameLobbys,
                                                        [accountName = user->accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames ();
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                        });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (setMaxCardValueInCreateGameLobbyObject.maxCardValue < 1)
        {
          return objectToStringWithObjectName (shared_class::SetMaxCardValueInCreateGameLobbyError{ .error = "maxCardValue < 1" });
        }
      else
        {
          gameLobbyWithAccount->gameOption.maxCardValue = setMaxCardValueInCreateGameLobbyObject.maxCardValue;
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxCardValueInCreateGameLobby{ .maxCardValue = setMaxCardValueInCreateGameLobbyObject.maxCardValue }));
          return {};
        }
    }
  else
    {
      return objectToStringWithObjectName (shared_class::SetMaxCardValueInCreateGameLobbyError{ .error = "could not find a game lobby for account" });
    }
}

std::optional<std::string>
leaveGameLobby (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  if (auto gameLobbyWithAccount = std::ranges::find_if (gameLobbys,
                                                        [accountName = user->accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames ();
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                        });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (gameLobbyWithAccount->removeUser (user))
        {
          if (gameLobbyWithAccount->accountCount () == 0)
            {
              gameLobbys.erase (gameLobbyWithAccount);
            }
          else
            {
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
              usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
              std::ranges::transform (gameLobbyWithAccount->accountNames (), std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ .accountName = accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
            }
        }
      return objectToStringWithObjectName (shared_class::LeaveGameLobbySuccess{});
    }
  else
    {
      return objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ .error = "could not remove user from lobby user not found in lobby" });
    }
}

std::optional<std::string>
relogTo (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys)
{
  // TODO relog into game and not only into lobby
  auto relogToObject = stringToObject<shared_class::RelogTo> (objectAsString);
  if (auto gameLobbyWithAccount = std::ranges::find_if (gameLobbys,
                                                        [accountName = user->accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames ();
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                        });
      gameLobbyWithAccount != gameLobbys.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          gameLobbyWithAccount->relogUser (user);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToSuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
          usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
          std::ranges::transform (gameLobbyWithAccount->accountNames (), std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ .accountName = accountName }; });
          return objectToStringWithObjectName (usersInGameLobby);
        }
      else
        {
          gameLobbyWithAccount->removeUser (user);
          if (gameLobbyWithAccount->accountCount () == 0)
            {
              gameLobbys.erase (gameLobbyWithAccount);
            }
          else
            {
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->gameLobbyName ();
              usersInGameLobby.durakGameOption.maxCardValue = gameLobbyWithAccount->gameOption.maxCardValue;
              std::ranges::transform (gameLobbyWithAccount->accountNames (), std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ .accountName = accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
              return {};
            }
        }
    }
  else if (relogToObject.wantsToRelog)
    {
      return objectToStringWithObjectName (shared_class::RelogToError{ .error = "trying to reconnect into game lobby but game lobby does not exist anymore" });
    }
  return {};
}

void
durakAttack (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAttackObject = stringToObject<shared_class::DurakAttack> (objectAsString);
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attack{ .playerName = user->accountName.value (), .cards{ std::move (durakAttackObject.cards) } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakDefend (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakDefendObject = stringToObject<shared_class::DurakDefend> (objectAsString);
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defend{ .playerName = user->accountName.value (), .cardToBeat{ durakDefendObject.cardToBeat }, .card{ durakDefendObject.card } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAttackPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attackPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAssistPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (assistPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAskDefendWantToTakeCardsAnswer (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAskDefendWantToTakeCardsAnswerObject = stringToObject<shared_class::DurakAskDefendWantToTakeCardsAnswer> (objectAsString);
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      if (durakAskDefendWantToTakeCardsAnswerObject.answer)
        {
          gameMachine->durakStateMachine.process_event (defendAnswerYes{ .playerName = user->accountName.value () });
        }
      else
        {
          gameMachine->durakStateMachine.process_event (defendAnswerNo{ .playerName = user->accountName.value () });
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
loginAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreLogin = true;
    }
}

void
createAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreCreateAccount = true;
    }
}
void
durakDefendPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defendPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakLeaveGame (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = std::ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return std::ranges::find_if (_game.getUsers (), [&accountName] (auto const &_user) { return _user->accountName.value () == accountName; }) != _game.getUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->removeUser (*user.get ());
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakLeaveGameError{ .error = "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}
