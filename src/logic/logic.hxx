#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <game_01_shared_class/serialization.hxx>
#include <string>
#include <vector>

boost::asio::awaitable<std::vector<std::string> > handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User> > &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

boost::asio::awaitable<std::string> createAccount (std::string objectAsString, boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

boost::asio::awaitable<std::string> loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User> > &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbys);

std::string logoutAccount (User &user);

std::string broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User> > &users, User const &sendingUser);

std::string joinChannel (std::string const &objectAsString, User &user);

std::string leaveChannel (std::string const &objectAsString, User &user);

std::string createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

std::optional<std::string> joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

std::optional<std::string> setMaxUserSizeInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

std::optional<std::string> leaveGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

std::optional<std::string> relogTo (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  return confu_soci::typeNameWithOutNamespace (typeToSend) + '|' + confu_boost::toString (typeToSend);
}

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
