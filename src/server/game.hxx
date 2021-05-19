#ifndef A9C5A17B_0A43_476F_9D91_9A6A4CC4B8E3
#define A9C5A17B_0A43_476F_9D91_9A6A4CC4B8E3

#include "durak/game.hxx"
#include "src/server/gameLobby.hxx"
#include <durak/gameData.hxx>

struct Game
{
public:
  Game (GameLobby &&gameLobby);

  std::vector<std::shared_ptr<User>> _users{};
  durak::Game durakGame;
  bool defendingPlayerWantsToDrawCardsFromTable{};
};

#endif /* A9C5A17B_0A43_476F_9D91_9A6A4CC4B8E3 */
